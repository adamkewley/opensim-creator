#include "simulator_screen.hpp"

#include "src/main_editor_state.hpp"
#include "src/opensim_bindings/simulation.hpp"
#include "src/ui/log_viewer.hpp"
#include "src/ui/component_3d_viewer.hpp"
#include "src/ui/main_menu.hpp"
#include "src/ui/component_details.hpp"
#include "src/ui/component_hierarchy.hpp"
#include "src/ui/help_marker.hpp"
#include "src/screens/model_editor_screen.hpp"
#include "src/assertions.hpp"
#include "src/utils/scope_guard.hpp"
#include "src/utils/os.hpp"

#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Common/ComponentOutput.h>
#include <imgui.h>
#include <nfd.h>

#include <limits>

using namespace osc;

namespace {

    // create Component 3D viewer for rendering the scene
    [[nodiscard]] std::unique_ptr<Component_3d_viewer> create_3dviewer() {
        return std::make_unique<Component_3d_viewer>(Component3DViewerFlags_Default | Component3DViewerFlags_DrawFrames);
    }


    // draw timescrubber slider
    void draw_simulation_scrubber(
            osc::Main_editor_state& st,
            Ui_simulation& focused_sim,
            fd::Report& focused_report) {

        double t0 = 0.0f;
        double tf = focused_sim.simulation.sim_final_time().count();
        double treport = focused_report.state.getTime();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        float v = static_cast<float>(treport);
        if (ImGui::SliderFloat("scrub", &v, static_cast<float>(t0), static_cast<float>(tf), "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            st.focused_simulation_scrubbing_time = v;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Left-Click: Change simulation time being shown");
            ImGui::TextUnformatted("Ctrl-Click: Type in the simulation time being shown");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    // select a simulation report based on scubbing time
    [[nodiscard]] fd::Report& select_report_based_on_scrubbing(
            Ui_simulation const& focused,
            float scrub_time) {

        auto& rr = focused.regular_reports;

        // if there are no regular reports, use the spot report
        if (rr.empty()) {
            return *focused.spot_report;
        }

        // if the scrub time is negative (a senteniel), use the
        // spot report
        if (scrub_time < 0.0) {
            return *focused.spot_report;
        }

        // search through the regular reports for the first report that
        // finishes equal-to or after the scrub time
        auto starts_after_or_eq_scrub_t = [&](std::unique_ptr<fd::Report> const& report) {
            return report->state.getTime() >= scrub_time;
        };

        auto it = std::find_if(rr.begin(), rr.end(), starts_after_or_eq_scrub_t);

        // if no such report is found, use the spot report
        if (it == rr.end()) {
            return *focused.spot_report;
        }

        return **it;
    }
}

struct osc::Simulator_screen::Impl final {
    // top-level state: shared between edit+sim screens
    std::shared_ptr<Main_editor_state> st;

    // scratch space for plots
    std::vector<float> plotscratch;

    // ui component state
    osc::ui::log_viewer::State log_viewer_st;
    osc::ui::main_menu::file_tab::State mm_filetab_st;

    Impl(std::shared_ptr<Main_editor_state> _st) : st {std::move(_st)} {
        // lazily init at least one viewer
        if (!st->viewers.front()) {
            st->viewers.front() = create_3dviewer();
        }
    }
};

// private Impl-related functions
namespace {

    // start a new simulation from whatever the user's currently editing
    void action_start_simulation_from_edited_model(osc::Main_editor_state& impl) {
        impl.start_simulating_edited_model();
    }

    // pop all updates from all active simulations
    void pop_all_simulator_updates(osc::Main_editor_state& impl) {
        for (auto& simulation : impl.simulations) {
            // pop regular reports
            {
                auto& rr = simulation->regular_reports;
                int popped = simulation->simulation.pop_regular_reports(rr);

                for (size_t i = rr.size() - static_cast<size_t>(popped); i < rr.size(); ++i) {
                    simulation->model->realizeReport(rr[i]->state);
                }
            }

            // pop latest spot report
            std::unique_ptr<fd::Report> new_spot_report = simulation->simulation.try_pop_latest_report();
            if (new_spot_report) {
                simulation->spot_report = std::move(new_spot_report);
                simulation->model->realizeReport(simulation->spot_report->state);
            }
        }
    }

    // draw details of one simulation
    void draw_simulation_progress_bar_etc(Simulator_screen::Impl& impl, int i) {
        Main_editor_state& st = *impl.st;

        if (!(0 <= i && i < static_cast<int>(st.simulations.size()))) {
            ImGui::TextUnformatted("(invalid simulation index)");
            return;
        }
        Ui_simulation& simulation = *st.simulations[i];

        ImGui::PushID(static_cast<int>(i));

        float progress = simulation.simulation.progress();
        ImVec4 base_color = progress >= 1.0f ? ImVec4{0.0f, 0.7f, 0.0f, 0.5f} : ImVec4{0.7f, 0.7f, 0.0f, 0.5f};
        if (static_cast<int>(i) == st.focused_simulation) {
            base_color.w = 1.0f;
        }

        bool should_erase = false;

        if (ImGui::Button("x")) {
            should_erase = true;
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, base_color);
        ImGui::ProgressBar(progress);
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            if (ImGui::IsKeyPressed(SDL_SCANCODE_DELETE)) {
                should_erase = true;
            }

            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() + 400.0f);
            ImGui::TextUnformatted(simulation.model->getName().c_str());
            ImGui::Dummy(ImVec2{0.0f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.7f, 0.7f, 0.7f, 1.0f});
            ImGui::Text("Wall time (sec): %.1f", simulation.simulation.wall_duration().count());
            ImGui::Text("Sim time (sec): %.1f", simulation.simulation.sim_current_time().count());
            ImGui::Text("Sim final time (sec): %.1f", simulation.simulation.sim_final_time().count());
            ImGui::Dummy(ImVec2{0.0f, 1.0f});
            ImGui::TextUnformatted("Left-click: Select this simulation");
            ImGui::TextUnformatted("Delete: cancel this simulation");
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            st.focused_simulation = static_cast<int>(i);
        }

        if (ImGui::BeginPopupContextItem("simcontextmenu")) {
            st.focused_simulation = static_cast<int>(i);

            if (ImGui::MenuItem("edit model")) {
                auto copy = std::make_unique<OpenSim::Model>(*simulation.model);
                st.set_model(std::move(copy));
                Application::current().request_transition<Model_editor_screen>(impl.st);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() + 400.0f);
                ImGui::TextUnformatted("Make the model initially used in this simulation into the model being edited in the editor");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            ImGui::EndPopup();
        }

        if (should_erase) {
            st.simulations.erase(st.simulations.begin() + i);
            if (static_cast<int>(i) <= st.focused_simulation) {
                --st.focused_simulation;
            }
        }

        ImGui::PopID();
    }

    // draw top-level "Simulation" tab that lists all simulations
    void draw_simulation_tab(Simulator_screen::Impl& impl) {
         osc::Main_editor_state& st = *impl.st;

        // draw scrubber for currently-selected sim
        ImGui::TextUnformatted("Scrubber:");
        ImGui::Separator();
        ImGui::Dummy(ImVec2{0.0f, 0.3f});
        Ui_simulation* sim = st.get_focused_sim();
        if (sim) {
            fd::Report& report = select_report_based_on_scrubbing(*sim, st.focused_simulation_scrubbing_time);
            draw_simulation_scrubber(st, *sim, report);
        } else {
            ImGui::TextDisabled("(no simulation selected)");
        }

        // draw simulations list
        ImGui::Dummy(ImVec2{0.0f, 1.0f});
        ImGui::TextUnformatted("Simulations:");
        ImGui::Separator();
        ImGui::Dummy(ImVec2{0.0f, 0.3f});
        for (size_t i = 0; i < st.simulations.size(); ++i) {
            draw_simulation_progress_bar_etc(impl, static_cast<int>(i));
        }
    }

// draw a plot for an integrator stat
#define OSC_DRAW_SIMSTAT_PLOT(statname, docstring) \
    {                                                                                                                  \
        scratch.clear();                                                                                               \
        for (auto const& report : focused.regular_reports) {                                                           \
            auto const& stats = report->stats;                                                                         \
            scratch.push_back(static_cast<float>(stats.statname));                                                     \
        }                                                                                                              \
        ImGui::TextUnformatted(#statname); \
        ImGui::SameLine();  \
        ui::help_marker::draw(docstring);  \
        ImGui::NextColumn();  \
        ImGui::PushID(imgui_id++);  \
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());  \
        ImGui::PlotLines("##"#statname, scratch.data(), static_cast<int>(scratch.size()), 0, nullptr, std::numeric_limits<float>::min(), std::numeric_limits<float>::max(), ImVec2(0.0f, .0f));                                 \
        ImGui::PopID(); \
        ImGui::NextColumn(); \
    }

    // draw top-level "simulation stats" tab that shows integrator stats etc. for
    // the focused simulation
    void draw_simulation_stats_tab(osc::Simulator_screen::Impl& impl) {
        Ui_simulation const* maybe_focused = impl.st->get_focused_sim();

        if (!maybe_focused) {
            ImGui::TextDisabled("(no simulation selected)");
            return;
        }
        Ui_simulation const& focused = *maybe_focused;

        ImGui::Dummy(ImVec2{0.0f, 1.0f});
        ImGui::TextUnformatted("parameters:");
        ImGui::SameLine();
        ui::help_marker::draw("The parameters used when this simulation was launched. These must be set *before* running the simulation");
        ImGui::Separator();
        ImGui::Dummy(ImVec2{0.0f, 2.0f});

        // draw simulation parameters
        {
            fd::Params const& p = focused.simulation.params();

            ImGui::Columns(2);

            ImGui::TextUnformatted("final_time");
            ImGui::SameLine();
            ui::help_marker::draw("The final time that the simulation should run until");
            ImGui::NextColumn();
            ImGui::Text("%f", p.final_time.count());
            ImGui::NextColumn();

            ImGui::TextUnformatted("throttle_to_wall_time");
            ImGui::SameLine();
            ui::help_marker::draw("Whether the simulation should slow down whenever it runs faster than wall-time");
            ImGui::NextColumn();
            ImGui::TextUnformatted(p.throttle_to_wall_time ? "true" : "false");
            ImGui::NextColumn();

            ImGui::TextUnformatted("integrator_method");
            ImGui::SameLine();
            ui::help_marker::draw("The integration method used by the underlying simulation engine");
            ImGui::NextColumn();
            ImGui::TextUnformatted(fd::g_IntegratorMethodNames[p.integrator_method]);
            ImGui::NextColumn();

            ImGui::TextUnformatted("reporting_interval");
            ImGui::SameLine();
            ui::help_marker::draw("The time interval, in simulation time, between data reports (e.g. for plots)");
            ImGui::NextColumn();
            ImGui::Text("%f", p.reporting_interval.count());
            ImGui::NextColumn();

            ImGui::TextUnformatted("integrator_step_limit");
            ImGui::SameLine();
            ui::help_marker::draw("The maximum number of internal integration steps that the simulation may take within a single call to the integrator's stepTo or stepBy function");
            ImGui::NextColumn();
            ImGui::Text("%i", p.integrator_step_limit);
            ImGui::NextColumn();

            ImGui::TextUnformatted("integrator_minimum_step_size");
            ImGui::SameLine();
            ui::help_marker::draw("The minimum step, in simulation time, that the integrator should attempt. Note: some integrators ignore this");
            ImGui::NextColumn();
            ImGui::Text("%f", p.integrator_minimum_step_size.count());
            ImGui::NextColumn();

            ImGui::TextUnformatted("integrator_maximum_step_size");
            ImGui::SameLine();
            ui::help_marker::draw("The maximum step, in simulation time, that the integrator can attempt. E.g. even if the integrator wants to take a larger step than this (because error control deemed so) the integrator can only take up to this limit. This does not affect reporting/plotting, which always happens at a regular time interval.");
            ImGui::NextColumn();
            ImGui::Text("%f", p.integrator_maximum_step_size.count());
            ImGui::NextColumn();

            ImGui::TextUnformatted("integrator_accuracy");
            ImGui::SameLine();
            ui::help_marker::draw("Accuracy of the integrator. This only does something if the integrator is error-controlled and able to dynamically improve simulation accuracy to match this parameter");
            ImGui::NextColumn();
            ImGui::Text("%f", p.integrator_accuracy);
            ImGui::NextColumn();

            ImGui::TextUnformatted("update_latest_state_on_every_step");
            ImGui::SameLine();
            ui::help_marker::draw("Whether the simulator should report to the UI on every integration step, rather than only at the reporting_interval. This is a tradeoff between perf. and UX. Updating the UI as often as possible results in a smooth 3D animation in the UI, but has more overhead. The cost of doing this is proportional to the FPS of the UI");
            ImGui::NextColumn();
            ImGui::TextUnformatted(p.update_latest_state_on_every_step ? "true" : "false");
            ImGui::NextColumn();

            ImGui::Columns();
        }

        ImGui::Dummy(ImVec2{0.0f, 10.0f});
        ImGui::TextUnformatted("plots:");
        ImGui::SameLine();
        ui::help_marker::draw("These plots are collected from the underlying simulation engine as the simulation runs. The data is heavily affected by the model's structure, choice of integrator, and simulation settings");
        ImGui::Separator();
        ImGui::Dummy(ImVec2{0.0f, 2.0f});

        // draw simulation stat plots
        {
            std::vector<float>& scratch = impl.plotscratch;
            int imgui_id = 0;

            ImGui::Columns(2);
            OSC_DRAW_SIMSTAT_PLOT(accuracyInUse, "Get the accuracy which is being used for error control.  Usually this is the same value that was specified to setAccuracy()");
            OSC_DRAW_SIMSTAT_PLOT(numConvergenceTestFailures, "Get the number of attempted steps that failed due to non-convergence of internal step iterations. This is most common with iterative methods but can occur if for some reason a step can't be completed.");
            OSC_DRAW_SIMSTAT_PLOT(numConvergentIterations, "For iterative methods, get the number of internal step iterations in steps that led to convergence (not necessarily successful steps).");
            OSC_DRAW_SIMSTAT_PLOT(numDivergentIterations, "For iterative methods, get the number of internal step iterations in steps that did not lead to convergence.");
            OSC_DRAW_SIMSTAT_PLOT(numErrorTestFailures, "Get the number of attempted steps that have failed due to the error being unacceptably high");
            OSC_DRAW_SIMSTAT_PLOT(numIterations, "For iterative methods, this is the total number of internal step iterations taken regardless of whether those iterations led to convergence or to successful steps. This is the sum of the number of convergent and divergent iterations which are available separately.");
            OSC_DRAW_SIMSTAT_PLOT(numProjectionFailures, "Get the number of attempted steps that have failed due to an error when projecting the state (either a Q- or U-projection)");
            OSC_DRAW_SIMSTAT_PLOT(numQProjections, "Get the total number of times a state positions Q have been projected");
            OSC_DRAW_SIMSTAT_PLOT(numQProjectionFailures, "Get the number of attempted steps that have failed due to an error when projecting the state positions (Q)");
            OSC_DRAW_SIMSTAT_PLOT(numRealizations, "Get the total number of state realizations that have been performed");
            OSC_DRAW_SIMSTAT_PLOT(numRealizationFailures, "Get the number of attempted steps that have failed due to an error when realizing the state");
            OSC_DRAW_SIMSTAT_PLOT(numStepsAttempted, "Get the total number of steps that have been attempted (successfully or unsuccessfully)");
            OSC_DRAW_SIMSTAT_PLOT(numStepsTaken, "Get the total number of steps that have been successfully taken");
            OSC_DRAW_SIMSTAT_PLOT(numUProjections, "Get the total number of times a state velocities U have been projected");
            OSC_DRAW_SIMSTAT_PLOT(numUProjectionFailures, "Get the number of attempted steps that have failed due to an error when projecting the state velocities (U)");
            OSC_DRAW_SIMSTAT_PLOT(predictedNextStepSize, "Get the step size that will be attempted first on the next call to stepTo() or stepBy().");
            ImGui::Columns();
        }
    }

    // action to take when user presses a key
    bool simscreen_on_keydown(osc::Simulator_screen::Impl& impl, SDL_KeyboardEvent const& e) {
        if (e.keysym.mod & KMOD_CTRL) {
            // Ctrl

            switch (e.keysym.sym) {
            case SDLK_e:
                // Ctrl + e
                Application::current().request_transition<Model_editor_screen>(std::move(impl.st));
                return true;
            }
        }
        return false;
    }

    // action to take when a generic event occurs
    bool simscreen_on_event(osc::Simulator_screen::Impl& impl, SDL_Event const& e) {
        if (e.type == SDL_KEYDOWN) {
            if (simscreen_on_keydown(impl, e.key)) {
                return true;
            }
        }

        bool handled = false;
        for (auto& viewer : impl.st->viewers) {
            if (!handled && viewer && viewer->is_moused_over()) {
                handled = viewer->on_event(e);
            }
        }

        return handled;
    }

    // draw output plots for the currently-focused sim
    void draw_output_plots(
            osc::Simulator_screen::Impl& impl,
            Ui_simulation const& focused_sim,
            fd::Report const& focused_report,
            OpenSim::Component const& selected) {

        int imgui_id = 0;

        ImGui::Columns(2);
        for (auto const& ptr : selected.getOutputs()) {
            OpenSim::AbstractOutput const& ao = *ptr.second;

            ImGui::TextUnformatted(ao.getName().c_str());
            ImGui::NextColumn();

            OpenSim::Output<double> const* od_ptr =
                dynamic_cast<OpenSim::Output<double> const*>(&ao);

            if (!od_ptr) {
                ImGui::TextUnformatted(ao.getValueAsString(focused_report.state).c_str());
                ImGui::NextColumn();
                continue;  // unplottable
            }

            OpenSim::Output<double> const& od = *od_ptr;

            size_t npoints = focused_sim.regular_reports.size();
            impl.plotscratch.resize(npoints);
            size_t i = 0;
            for (auto const& report : focused_sim.regular_reports) {
                double v = od.getValue(report->state);
                impl.plotscratch[i++] = static_cast<float>(v);
            }

            ImGui::PushID(imgui_id++);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            ImGui::PlotLines("##nolabel", impl.plotscratch.data(), static_cast<int>(impl.plotscratch.size()), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2{0.0f, 20.0f});

            if (ImGui::BeginPopupContextItem(od.getName().c_str())) {
                if (ImGui::MenuItem("Add to outputs watch")) {
                    impl.st->desired_outputs.emplace_back(selected, od);
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns();
    }

    // draw "selection" tab, which shows details of current selection
    void draw_selection_tab(osc::Simulator_screen::Impl& impl) {

        Ui_simulation const* maybe_sim = impl.st->get_focused_sim();

        if (!maybe_sim) {
            ImGui::TextDisabled("(no simulation selected)");
            return;
        }
        Ui_simulation const& sim = *maybe_sim;

        if (!sim.selected) {
            ImGui::TextDisabled("(no component selected)");
            return;
        }

        fd::Report const& report = select_report_based_on_scrubbing(sim, impl.st->focused_simulation_scrubbing_time);

        ui::component_details::draw(report.state, sim.selected);

        if (ImGui::CollapsingHeader("outputs")) {
            draw_output_plots(impl, sim, report, *sim.selected);
        }
    }

    // export a timeseries to a CSV file and return the filepath
    std::string export_timeseries_to_csv(
            float const* ts,    // times
            float const* vs,    // values @ each time in times
            size_t n,           // number of datapoints
            char const* vname   // name of values (header)
            ) {

        nfdchar_t* outpath = nullptr;
        nfdresult_t result = NFD_SaveDialog("csv", nullptr, &outpath);
        OSC_SCOPE_GUARD_IF(outpath != nullptr, { free(outpath); });

        if (result != NFD_OKAY) {
            return "";  // user cancelled out
        }

        std::ofstream fout{outpath};

        if (!fout) {
            log::error("%s: error opening file for writing", outpath);
            return "";  // error opening output file for writing
        }

        fout << "t," << vname << '\n';
        for (size_t i = 0; i < n; ++i) {
            fout << ts[i] << ',' << vs[i] << '\n';
        }

        if (!fout) {
            log::error("%s: error encountered while writing CSV data to file", outpath);
            return "";  // error writing
        }

        log::info("%: successfully wrote CSV data to output file", outpath);

        return std::string{outpath};
    }

    // export all plotted outputs to a single CSV file and return the filepath
    std::string export_all_plotted_outputs_to_csv(
            Simulator_screen::Impl const& impl,
            Ui_simulation const& sim) {

        // try prompt user for save location
        nfdchar_t* outpath = nullptr;
        nfdresult_t result = NFD_SaveDialog("csv", nullptr, &outpath);
        OSC_SCOPE_GUARD_IF(outpath != nullptr, { free(outpath); });

        if (result != NFD_OKAY) {
            return "";  // user cancelled out
        }

        // try to open the output file
        std::ofstream fout{outpath};
        if (!fout) {
            log::error("%s: error opening file for writing", outpath);
            return "";  // error opening output file for writing
        }

        struct Plottable_output final {
            OpenSim::Component const& component;
            OpenSim::AbstractOutput const& output;
            extrator_fn_t extractor;

            Plottable_output(
                OpenSim::Component const& _component,
                OpenSim::AbstractOutput const& _output,
                extrator_fn_t _extractor) :

                component{_component},
                output{_output},
                extractor{_extractor} {
            }
        };

        // collect plottable outputs
        std::vector<Plottable_output> plottable_outputs;
        for (Desired_output const& de : impl.st->desired_outputs) {
            if (!de.extractor) {
                continue;  // no extractor function
            }

            OpenSim::Component const* cp = nullptr;
            try {
                cp = &sim.model->getComponent(de.component_path);
            } catch (...) {
                // OpenSim, innit
                //
                // avoid OpenSim::Component::findComponent like the plague
                // because it's written by someone who seems to be pathalogically
                // allergic to performance
            }

            if (!cp) {
                continue;  // the component doesn't exist
            }

            OpenSim::Component const& c = *cp;

            OpenSim::AbstractOutput const* aop = nullptr;
            try {
                aop = &c.getOutput(de.output_name);
            } catch (...) {
                // OpenSim, innit
            }

            if (!aop) {
                continue;  // the output doesn't exist on the component
            }

            size_t typehash = typeid(*aop).hash_code();

            if (typehash != de.typehash) {
                continue;  // the output is there, but now has a different type
            }

            plottable_outputs.emplace_back(c, *aop, de.extractor);
        }

        // write header line
        fout << "time";
        for (Plottable_output const& po : plottable_outputs) {
            fout << ',' << po.component.getName() << '_' << po.output.getName();
        }
        fout << '\n';

        // write data lines
        for (auto const& report : sim.regular_reports) {
            SimTK::State const& stkst = report->state;

            // write time
            fout << stkst.getTime();
            for (Plottable_output const& po : plottable_outputs) {
                fout << ',' << po.extractor(po.output, stkst);
            }
            fout << '\n';
        }

        // check writing was successful
        //
        // this is just a sanity check: it will be written regardless
        if (!fout) {
            log::warn("%s: encountered error while writing output data: some of the data may have been written, but maybe not all of it", outpath);
        }

        return std::string{outpath};
    }

    // draw "outputs" tab, which shows user-selected simulation outputs
    void draw_outputs_tab(osc::Simulator_screen::Impl& impl) {

        Ui_simulation const* maybe_sim = impl.st->get_focused_sim();
        if (!maybe_sim) {
            ImGui::TextDisabled("(no simulation selected)");
            return;
        }
        Ui_simulation const& sim = *maybe_sim;

        fd::Report const& report = select_report_based_on_scrubbing(sim, impl.st->focused_simulation_scrubbing_time);

        int imgui_id = 0;
        Main_editor_state& st = *impl.st;

        if (st.desired_outputs.empty()) {
            ImGui::TextUnformatted("No outputs being plotted: right-click them in the model editor");
            return;
        }

        if (ImGui::Button("Save all to CSV")) {
            export_all_plotted_outputs_to_csv(impl, sim);
        }

        ImGui::SameLine();

        if (ImGui::Button("Save all to CSV & Open")) {
            std::string path = export_all_plotted_outputs_to_csv(impl, sim);
            if (!path.empty()) {
                open_path_in_default_application(path);
            }
        }

        ImGui::Dummy(ImVec2{0.0f, 5.0f});

        ImGui::Columns(2);
        for (Desired_output const& de : st.desired_outputs) {
            ImGui::Text("%s[%s]", de.component_path.c_str(), de.output_name.c_str());
            ImGui::NextColumn();

            OpenSim::Component const* cp = nullptr;
            try {
                cp = &sim.model->getComponent(de.component_path);
            } catch (...) {
                // OpenSim, innit
                //
                // avoid OpenSim::Component::findComponent like the plague
                // because it's written by someone who seems to be pathalogically
                // allergic to performance
            }

            if (!cp) {
                ImGui::TextUnformatted("component not found");
                ImGui::NextColumn();
                continue;
            }

            OpenSim::Component const& c = *cp;

            OpenSim::AbstractOutput const* aop = nullptr;
            try {
                aop = &c.getOutput(de.output_name);
            } catch (...) {
                // OpenSim, innit
            }

            if (!aop) {
                ImGui::TextUnformatted("output not found");
                ImGui::NextColumn();
                continue;
            }

            OpenSim::AbstractOutput const& ao = *aop;
            size_t typehash = typeid(ao).hash_code();

            if (typehash != de.typehash) {
                ImGui::TextUnformatted("output type changed");
                ImGui::NextColumn();
                continue;
            }

            if (!de.extractor) {
                // no extractor function, so unplottable
                ImGui::TextUnformatted(ao.getValueAsString(report.state).c_str());
                ImGui::NextColumn();
                continue;
            }

            // else: it's a plottable output

            size_t npoints = sim.regular_reports.size();
            impl.plotscratch.resize(npoints);
            size_t i = 0;
            for (auto const& r : sim.regular_reports) {
                double v = de.extractor(ao, r->state);
                impl.plotscratch[i++] = static_cast<float>(v);
            }

            ImGui::PushID(imgui_id++);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            ImGui::PlotLines("##nolabel", impl.plotscratch.data(), static_cast<int>(impl.plotscratch.size()));
            if (ImGui::BeginPopupContextItem("outputplotscontextmenu")) {
                if (ImGui::MenuItem("Save as CSV")) {
                    std::vector<float> ts;
                    ts.reserve(sim.regular_reports.size());
                    for (auto const& r : sim.regular_reports) {
                        ts.push_back(static_cast<float>(r->state.getTime()));
                    }
                    OSC_ASSERT_ALWAYS(ts.size() == impl.plotscratch.size());
                    export_timeseries_to_csv(ts.data(), impl.plotscratch.data(), ts.size(), ao.getName().c_str());
                }

                if (ImGui::MenuItem("Save as CSV & Open")) {
                    std::vector<float> ts;
                    ts.reserve(sim.regular_reports.size());
                    for (auto const& r : sim.regular_reports) {
                        ts.push_back(static_cast<float>(r->state.getTime()));
                    }
                    OSC_ASSERT_ALWAYS(ts.size() == impl.plotscratch.size());
                    std::string outpath = export_timeseries_to_csv(ts.data(), impl.plotscratch.data(), ts.size(), ao.getName().c_str());

                    if (!outpath.empty()) {
                        open_path_in_default_application(outpath);
                    }
                }

                ImGui::EndPopup();
            }
            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::Columns();
    }

    // draw "hierarchy" tab, which shows the tree hierarchy structure for
    // the currently-focused sim
    void draw_hierarchy_tab(Simulator_screen::Impl& impl) {
        Ui_simulation* maybe_sim = impl.st->get_focused_sim();

        if (!maybe_sim) {
            ImGui::TextDisabled("(no simulation selected)");
            return;
        }
        Ui_simulation& sim = *maybe_sim;

        auto resp = ui::component_hierarchy::draw(sim.model.get(), sim.selected, sim.hovered);

        if (resp.type == ui::component_hierarchy::SelectionChanged) {
            sim.selected = const_cast<OpenSim::Component*>(resp.ptr);
        } else if (resp.type == ui::component_hierarchy::HoverChanged) {
            sim.hovered = const_cast<OpenSim::Component*>(resp.ptr);
        }
    }

    // draw a 3D model viewer
    void draw_3dviewer(
            Ui_simulation& sim,
            fd::Report const& report,
            Component_3d_viewer& viewer,
            char const* name) {

        Component3DViewerResponse resp =
            viewer.draw(name, *sim.model, report.state, sim.selected, sim.hovered);

        if (resp.is_left_clicked && resp.hovertest_result) {
            sim.selected = const_cast<OpenSim::Component*>(resp.hovertest_result);
        }
        if (resp.is_moused_over && resp.hovertest_result != sim.hovered) {
            sim.hovered = const_cast<OpenSim::Component*>(resp.hovertest_result);
        }
    }

    // draw all active 3D viewers
    //
    // the user can (de)activate 3D viewers in the "Window" tab
    void draw_all_3dviewers(Simulator_screen::Impl& impl) {
        Ui_simulation* maybe_sim = impl.st->get_focused_sim();

        if (!maybe_sim) {
            if (ImGui::Begin("render")) {
                ImGui::TextDisabled("(no simulation selected)");
            }
            ImGui::End();
            return;
        }

        Ui_simulation& sim = *maybe_sim;
        fd::Report const& report = select_report_based_on_scrubbing(sim, impl.st->focused_simulation_scrubbing_time);
        Main_editor_state& st = *impl.st;

        for (size_t i = 0; i < st.viewers.size(); ++i) {
            auto& maybe_viewer = st.viewers[i];

            if (!maybe_viewer) {
                continue;
            }

            Component_3d_viewer& viewer = *maybe_viewer;

            char buf[64];
            std::snprintf(buf, sizeof(buf), "viewer%zu", i);

            draw_3dviewer(sim, report, viewer, buf);
        }
    }

    // draw the simulator screen
    void simscreen_draw(osc::Simulator_screen::Impl& impl) {

        // draw main menu
        if (ImGui::BeginMainMenuBar()) {
            ui::main_menu::file_tab::draw(impl.mm_filetab_st, impl.st);
            ui::main_menu::window_tab::draw(*impl.st);
            ui::main_menu::about_tab::draw();

            ImGui::Dummy(ImVec2{5.0f, 0.0f});

            if (ImGui::Button(ICON_FA_CUBE " Switch to editor (Ctrl+E)")) {

                // request the transition then exit this drawcall ASAP
                Application::current().request_transition<Model_editor_screen>(std::move(impl.st));
                ImGui::EndMainMenuBar();
                return;
            }

            ImGui::EndMainMenuBar();
        }

        Main_editor_state& st = *impl.st;

        // edge-case: there are no simulations available, so
        // show a "you need to run something, fool" dialog
        if (!st.has_simulations()) {
            if (ImGui::Begin("Warning")) {
                ImGui::TextUnformatted("No simulations are currently running");
                if (ImGui::Button("Run new simulation")) {
                    action_start_simulation_from_edited_model(st);
                }
            }
            ImGui::End();
            return;
        }

        // draw simulations tab
        if (st.showing.simulations) {
            if (ImGui::Begin("Simulations", &st.showing.simulations)) {
                draw_simulation_tab(impl);
            }
            ImGui::End();
        }

        // draw 3d viewers
        {
            draw_all_3dviewers(impl);
        }

        // draw hierarchy tab
        if (st.showing.hierarchy) {
            if (ImGui::Begin("Hierarchy", &st.showing.hierarchy)) {
                draw_hierarchy_tab(impl);
            }
            ImGui::End();
        }

        // draw selection tab
        if (st.showing.selection_details) {
            if (ImGui::Begin("Selection", &st.showing.selection_details)) {
                draw_selection_tab(impl);
            }
            ImGui::End();
        }

        // draw outputs tab
        if (st.showing.outputs) {
            if (ImGui::Begin("Outputs", &st.showing.outputs)) {
                draw_outputs_tab(impl);
            }
            ImGui::End();
        }

        // draw simulation stats tab
        if (st.showing.simulation_stats) {
            if (ImGui::Begin("Simulation Details", &st.showing.simulation_stats)) {
                draw_simulation_stats_tab(impl);
            }
            ImGui::End();
        }

        // draw log tab
        if (st.showing.log) {
            if (ImGui::Begin("Log", &st.showing.log, ImGuiWindowFlags_MenuBar)) {
                ui::log_viewer::draw(impl.log_viewer_st);
            }
            ImGui::End();
        }
    }
}

// Simulator_screen: public impl.

osc::Simulator_screen::Simulator_screen(std::shared_ptr<Main_editor_state> st) :
    impl{new Impl{std::move(st)}} {
}

osc::Simulator_screen::~Simulator_screen() noexcept = default;

bool osc::Simulator_screen::on_event(SDL_Event const& e) {
    return ::simscreen_on_event(*impl, e);
}

void osc::Simulator_screen::tick(float) {
    pop_all_simulator_updates(*impl->st);
}

void osc::Simulator_screen::draw() {
    ::simscreen_draw(*impl);
}
