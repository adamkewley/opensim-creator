#include "UiFdSimulation.hpp"

#include "src/OpenSimBindings/BasicModelStatePair.hpp"
#include "src/OpenSimBindings/SimulationClock.hpp"
#include "src/OpenSimBindings/SimulationReport.hpp"
#include "src/OpenSimBindings/SimulationStatus.hpp"
#include "src/OpenSimBindings/VirtualSimulation.hpp"
#include "src/OpenSimBindings/FdSimulation.hpp"
#include "src/Platform/App.hpp"
#include "src/Utils/Algorithms.hpp"
#include "src/Utils/UID.hpp"
#include "src/Utils/SynchronizedValue.hpp"

#include <nonstd/span.hpp>
#include <OpenSim/Simulation/Model/Model.h>

#include <mutex>
#include <optional>
#include <memory>
#include <vector>

// helper function for creating a simulator that's hooked up to the reports vector
static osc::FdSimulation MakeSimulation(
        osc::BasicModelStatePair p,
        osc::SynchronizedValue<osc::BasicModelStatePair>& uiModelState,
        osc::FdParams const& params,
        osc::SynchronizedValue<std::vector<osc::SimulationReport>>& reports)
{
    auto callback = [&](osc::SimulationReport r)
    {
        {
            auto modelGuard = uiModelState.lock();
            modelGuard->getModel().realizeReport(r.updStateHACK());
        }
        {
            auto reportsGuard = reports.lock();
            reportsGuard->push_back(std::move(r));
        }
        osc::App::cur().requestRedraw();
    };
    return osc::FdSimulation{std::move(p), params, std::move(callback)};
}

static std::vector<osc::OutputExtractor> GetFdSimulatorOutputExtractorsAsVector()
{
    std::vector<osc::OutputExtractor> rv;
    int nOutputExtractors = osc::GetNumFdSimulatorOutputExtractors();
    rv.reserve(nOutputExtractors);
    for (int i = 0; i < nOutputExtractors; ++i)
    {
        rv.push_back(osc::GetFdSimulatorOutputExtractor(i));
    }
    return rv;
}

class osc::UiFdSimulation::Impl final {
public:

    Impl(BasicModelStatePair p, FdParams const& params) :
        m_ModelState{std::move(p)},
        m_Simulation{MakeSimulation(*m_ModelState.lock(), m_ModelState, params, m_Reports)},
        m_ParamsAsParamBlock{ToParamBlock(params)},
        m_SimulatorOutputExtractors(GetFdSimulatorOutputExtractorsAsVector())
    {
    }

    SynchronizedValueGuard<OpenSim::Model const> getModel() const
    {
        OpenSim::Model const& ref = m_ModelState.lock()->getModel();
        return {m_ModelState.mutex(), ref};
    }

    int getNumReports() const
    {
        return static_cast<int>(m_Reports.lock()->size());
    }

    SimulationReport getSimulationReport(int reportIndex) const
    {
        return m_Reports.lock()->at(reportIndex);
    }

    std::vector<SimulationReport> getAllSimulationReports() const
    {
        return *m_Reports.lock();
    }

    SimulationStatus getStatus() const
    {
        return m_Simulation.getStatus();
    }

    SimulationClock::time_point getCurTime() const
    {
        auto guard = m_Reports.lock();

        if (!guard->empty())
        {
            return SimulationClock::start() + SimulationClock::duration{guard->back().getState().getTime()};
        }
        else
        {
            return getStartTime();
        }
    }

    SimulationClock::time_point getStartTime() const
    {
        return SimulationClock::start() + SimulationClock::duration{m_ModelState.lock()->getState().getTime()};
    }

    SimulationClock::time_point getEndTime() const
    {
        return m_Simulation.params().FinalTime;
    }

    float getProgress() const
    {
        auto start = getStartTime();
        auto end = getEndTime();
        auto cur = getCurTime();
        return static_cast<float>((cur-start)/(end-start));
    }

    ParamBlock const& getParams() const
    {
        return m_ParamsAsParamBlock;
    }

    nonstd::span<OutputExtractor const> getOutputExtractors() const
    {
        return m_SimulatorOutputExtractors;
    }

    void requestStop()
    {
        m_Simulation.requestStop();
    }

    void stop()
    {
        m_Simulation.stop();
    }

private:
    SynchronizedValue<BasicModelStatePair> m_ModelState;  // HACK: state has to be re-realized by UI thread
    SynchronizedValue<std::vector<SimulationReport>> m_Reports;
    FdSimulation m_Simulation;
    ParamBlock m_ParamsAsParamBlock;
    std::vector<OutputExtractor> m_SimulatorOutputExtractors;
};


// public API

osc::UiFdSimulation::UiFdSimulation(BasicModelStatePair ms,FdParams const& params) :
    m_Impl{std::make_unique<Impl>(std::move(ms), params)}
{
}
osc::UiFdSimulation::UiFdSimulation(UiFdSimulation&&) noexcept = default;
osc::UiFdSimulation& osc::UiFdSimulation::operator=(UiFdSimulation&&) noexcept = default;
osc::UiFdSimulation::~UiFdSimulation() noexcept = default;

osc::SynchronizedValueGuard<OpenSim::Model const> osc::UiFdSimulation::getModel() const
{
    return m_Impl->getModel();
}

int osc::UiFdSimulation::getNumReports() const
{
    return m_Impl->getNumReports();
}

osc::SimulationReport osc::UiFdSimulation::getSimulationReport(int reportIndex) const
{
    return m_Impl->getSimulationReport(std::move(reportIndex));
}

std::vector<osc::SimulationReport> osc::UiFdSimulation::getAllSimulationReports() const
{
    return m_Impl->getAllSimulationReports();
}

osc::SimulationStatus osc::UiFdSimulation::getStatus() const
{
    return m_Impl->getStatus();
}

osc::SimulationClock::time_point osc::UiFdSimulation::getCurTime() const
{
    return m_Impl->getCurTime();
}

osc::SimulationClock::time_point osc::UiFdSimulation::getStartTime() const
{
    return m_Impl->getStartTime();
}

osc::SimulationClock::time_point osc::UiFdSimulation::getEndTime() const
{
    return m_Impl->getEndTime();
}

float osc::UiFdSimulation::getProgress() const
{
    return m_Impl->getProgress();
}

osc::ParamBlock const& osc::UiFdSimulation::getParams() const
{
    return m_Impl->getParams();
}

nonstd::span<osc::OutputExtractor const> osc::UiFdSimulation::getOutputExtractors() const
{
    return m_Impl->getOutputExtractors();
}

void osc::UiFdSimulation::requestStop()
{
    return m_Impl->requestStop();
}

void osc::UiFdSimulation::stop()
{
    return m_Impl->stop();
}
