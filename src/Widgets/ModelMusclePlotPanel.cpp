#include "ModelMusclePlotPanel.hpp"

#include "src/Bindings/ImGuiHelpers.hpp"
#include "src/OpenSimBindings/AutoFinalizingModelStatePair.hpp"
#include "src/OpenSimBindings/CoordinateEdit.hpp"
#include "src/OpenSimBindings/ModelStateCommit.hpp"
#include "src/OpenSimBindings/OpenSimHelpers.hpp"
#include "src/OpenSimBindings/UndoableModelStatePair.hpp"
#include "src/Platform/Log.hpp"
#include "src/Utils/Assertions.hpp"
#include "src/Utils/Algorithms.hpp"
#include "src/Utils/CStringView.hpp"
#include "src/Utils/UID.hpp"

#include <imgui.h>
#include <implot.h>
#include <OpenSim/Common/ComponentPath.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/Muscle.h>

#include <memory>
#include <string_view>
#include <sstream>
#include <utility>

static bool SortByComponentName(OpenSim::Component const* p1, OpenSim::Component const* p2)
{
	return p1->getName() < p2->getName();
}

namespace
{
	class MuscleOutput {
	public:
		MuscleOutput(char const* name, char const* units, double(*getter)(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)) :
			m_Name{std::move(name)},
			m_Units{std::move(units)},
			m_Getter{std::move(getter)}
		{
		}

		char const* getName() const
		{
			return m_Name.c_str();
		}

		char const* getUnits()
		{
			return m_Units.c_str();
		}

		double operator()(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c) const
		{
			return m_Getter(st, muscle, c);
		}

	private:
		friend bool operator<(MuscleOutput const&, MuscleOutput const&);
		friend bool operator==(MuscleOutput const&, MuscleOutput const&);
		friend bool operator!=(MuscleOutput const&, MuscleOutput const&);

		osc::CStringView m_Name;
		osc::CStringView m_Units;
		double(*m_Getter)(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c);
	};

	bool operator<(MuscleOutput const& a, MuscleOutput const& b)
	{
		return a.m_Name < b.m_Name;
	}

	bool operator==(MuscleOutput const& a, MuscleOutput const& b)
	{
		return a.m_Name == b.m_Name && a.m_Units == b.m_Units && a.m_Getter == b.m_Getter;
	}

	bool operator!=(MuscleOutput const& a, MuscleOutput const& b)
	{
		return !(a == b);
	}
}

static double GetMomentArm(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getGeometryPath().computeMomentArm(st, c);
}

static double GetFiberLength(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberLength(st);
}

static double GetTendonLength(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonLength(st);
}

static double GetPennationAngle(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return glm::degrees(muscle.getPennationAngle(st));
}

static double GetNormalizedFiberLength(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getNormalizedFiberLength(st);
}

static double GetTendonStrain(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonStrain(st);
}

static double GetFiberPotentialEnergy(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberPotentialEnergy(st);
}

static double GetTendonPotentialEnergy(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonPotentialEnergy(st);
}

static double GetMusclePotentialEnergy(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getMusclePotentialEnergy(st);
}

static double GetTendonForce(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonForce(st);
}

static double GetActiveFiberForce(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getActiveFiberForce(st);
}

static double GetPassiveFiberForce(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getPassiveFiberForce(st);
}

static double GetTotalFiberForce(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberForce(st);
}

static double GetFiberStiffness(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberStiffness(st);
}

static double GetFiberStiffnessAlongTendon(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberStiffnessAlongTendon(st);
}

static double GetTendonStiffness(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonStiffness(st);
}

static double GetMuscleStiffness(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getMuscleStiffness(st);
}

static double GetFiberActivePower(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberActivePower(st);
}

static double GetFiberPassivePower(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getFiberActivePower(st);
}

static double GetTendonPower(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonPower(st);
}

static double GetMusclePower(SimTK::State const& st, OpenSim::Muscle const& muscle, OpenSim::Coordinate const& c)
{
	return muscle.getTendonPower(st);
}

static MuscleOutput GetDefaultMuscleOutput()
{
	return MuscleOutput{"Moment Arm", "Unitless", GetMomentArm};
}

static std::vector<MuscleOutput> InitMuscleOutputs()
{
	std::vector<MuscleOutput> rv =
	{{
		GetDefaultMuscleOutput(),
		{"Tendon Length", "m", GetTendonLength},
		{"Fiber Length", "m", GetFiberLength},
		{"Pennation Angle", "deg", GetPennationAngle},
		{"Normalized Fiber Length", "Unitless", GetNormalizedFiberLength},
		{"Tendon Strain", "Unitless", GetTendonStrain},
		{"Fiber Potential Energy", "J", GetFiberPotentialEnergy},
		{"Tendon Potential Energy", "J", GetTendonPotentialEnergy},
		{"Muscle Potential Energy", "J", GetMusclePotentialEnergy},
		{"Tendon Force", "N", GetTendonForce},
		{"Active Fiber Force", "N", GetActiveFiberForce},
		{"Passive Fiber Force", "N", GetPassiveFiberForce},
		{"Total Fiber Force", "N", GetTotalFiberForce},
		{"Fiber Stiffness", "N/m", GetFiberStiffness},
		{"Fiber Stiffness Along Tendon", "N/m", GetFiberStiffnessAlongTendon},
		{"Tendon Stiffness", "N/m", GetTendonStiffness},
		{"Muscle Stiffness", "N/m", GetMuscleStiffness},
		{"Fiber Active Power", "W", GetFiberActivePower},
		{"Fiber Passive Power", "W", GetFiberPassivePower},
		{"Tendon Power", "W", GetTendonPower},
		{"Muscle Power", "W", GetMusclePower},
	}};
	std::sort(rv.begin(), rv.end());
	return rv;
}

static std::vector<MuscleOutput> const& GetMuscleOutputs()
{
	static std::vector<MuscleOutput> g_MuscleOutputs = InitMuscleOutputs();
	return g_MuscleOutputs;
}

// state stuff
namespace
{
	// data that is shared between all states
	struct SharedStateData final {
		explicit SharedStateData(std::shared_ptr<osc::UndoableModelStatePair> uim) : Uim{std::move(uim)}
		{
			OSC_ASSERT(Uim != nullptr);
		}

		SharedStateData(std::shared_ptr<osc::UndoableModelStatePair> uim,
			            OpenSim::ComponentPath const& coordPath,
			            OpenSim::ComponentPath const& musclePath) :
			Uim{std::move(uim)},
			RequestedMuscleComponentPath{musclePath},
			RequestedCoordinateComponentPath{coordPath}
		{
			OSC_ASSERT(Uim != nullptr);
		}

		std::shared_ptr<osc::UndoableModelStatePair> Uim;
		int RequestedNumPlotPoints = 180;
		MuscleOutput RequestedMuscleOutput = GetDefaultMuscleOutput();
		OpenSim::ComponentPath RequestedMuscleComponentPath;
		OpenSim::ComponentPath RequestedCoordinateComponentPath;
	};

	// base class for a single widget state
	class MusclePlotState {
	protected:
		MusclePlotState(SharedStateData* shared_) : shared{std::move(shared_)}
		{
			OSC_ASSERT(shared != nullptr);
		}
	public:
		virtual ~MusclePlotState() noexcept = default;
		virtual std::unique_ptr<MusclePlotState> draw() = 0;

	protected:
		SharedStateData* shared;
	};

	std::unique_ptr<MusclePlotState> CreateChooseCoordinateState(SharedStateData*);
	std::unique_ptr<MusclePlotState> CreateChooseMuscleState(SharedStateData*);

	// state in which the plot is being shown to the user
	class ShowingPlotState final : public MusclePlotState {
	public:
		explicit ShowingPlotState(SharedStateData* shared_) : MusclePlotState{std::move(shared_)}
		{
		}

		std::unique_ptr<MusclePlotState> draw() override
		{
			OpenSim::Model const& model = shared->Uim->getModel();

			OpenSim::Coordinate const* coord = osc::FindComponent<OpenSim::Coordinate>(model, shared->RequestedCoordinateComponentPath);
			if (!coord)
			{
				return CreateChooseCoordinateState(shared);
			}

			OpenSim::Muscle const* muscle = osc::FindComponent<OpenSim::Muscle>(model, shared->RequestedMuscleComponentPath);
			if (!muscle)
			{
				return CreateChooseMuscleState(shared);
			}

			if (shared->Uim->getModelVersion() != m_LastPlotModelVersion ||
				shared->Uim->getStateVersion() != m_LastPlotStateVersion ||
				shared->RequestedMuscleOutput != m_ActiveMuscleOutput ||
				shared->RequestedNumPlotPoints != m_NumPlotPointsPlotted)
			{
				recomputePlotData(*coord, *muscle);
			}

			if (m_YValues.empty())
			{
				ImGui::Text("(no Y values)");
				return nullptr;
			}

			glm::vec2 availSize = ImGui::GetContentRegionAvail();

			std::string title = computePlotTitle(*coord);
			std::string xAxisLabel = computePlotXAxisTitle(*coord);
			std::string yAxisLabel = computePlotYAxisTitle();

			double currentX = osc::ConvertCoordValueToDisplayValue(*coord, coord->getValue(shared->Uim->getState()));

			bool isHovered = false;
			ImPlotPoint p = {};
			if (ImPlot::BeginPlot(title.c_str(), availSize, ImPlotFlags_AntiAliased | ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoChild | ImPlotFlags_NoFrame))
			{
				ImPlotAxisFlags xAxisFlags = ImPlotAxisFlags_AutoFit;
				ImPlotAxisFlags yAxisFlags = ImPlotAxisFlags_AutoFit;
				ImPlot::SetupAxes(xAxisLabel.c_str(), yAxisLabel.c_str(), xAxisFlags, yAxisFlags);
				ImPlot::PlotLine(shared->RequestedMuscleComponentPath.getComponentName().c_str(),
					             m_XValues.data(),
					             m_YValues.data(),
					             static_cast<int>(m_XValues.size()));
				ImPlot::TagX(currentX, { 1.0f, 1.0f, 1.0f, 1.0f });
				isHovered = ImPlot::IsPlotHovered();
				p = ImPlot::GetPlotMousePos();
				if (isHovered)
				{
					ImPlot::TagX(p.x, { 1.0f, 1.0f, 1.0f, 0.6f });
				}
				ImPlot::EndPlot();
			}
			if (isHovered && ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				osc::CoordinateEdit edit
				{
					osc::ConvertCoordDisplayValueToStorageValue(*coord, static_cast<float>(p.x)),
					coord->getSpeedValue(shared->Uim->getState()),
					coord->getLocked(shared->Uim->getState())
				};
				shared->Uim->updUiModel().pushCoordinateEdit(*coord, edit);
			}
			if (ImGui::BeginPopupContextItem((title + "_contextmenu").c_str()))
			{
				drawPlotDataTypeSelector();
				if (ImGui::InputInt("num data points", &m_NumPlotPointsEdited, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					shared->RequestedNumPlotPoints = m_NumPlotPointsEdited;
				}
				ImGui::EndPopup();
			}

			return nullptr;
		}

	private:
		void drawPlotDataTypeSelector()
		{
			nonstd::span<MuscleOutput const> allOutputs = GetMuscleOutputs();

			std::vector<char const*> names;
			int active = -1;
			for (int i = 0; i < static_cast<int>(allOutputs.size()); ++i)
			{
				MuscleOutput const& o = allOutputs[i];
				names.push_back(o.getName());
				if (o == m_ActiveMuscleOutput)
				{
					active = i;
				}
			}

			if (ImGui::Combo("data type", &active, names.data(), static_cast<int>(names.size())))
			{
				shared->RequestedMuscleOutput = allOutputs[active];
			}
		}

		void recomputePlotData(OpenSim::Coordinate const& coord, OpenSim::Muscle const& muscle)
		{
			if (shared->RequestedNumPlotPoints <= 0)
			{
				m_LastPlotModelVersion = shared->Uim->getModelVersion();
				m_LastPlotStateVersion = shared->Uim->getStateVersion();
				m_ActiveMuscleOutput = shared->RequestedMuscleOutput;
				m_NumPlotPointsPlotted = shared->RequestedNumPlotPoints;

				return;
			}

			OpenSim::Model model = shared->Uim->getModel();
			model.buildSystem();
			model.initializeState();

			// input state
			SimTK::State stateCopy = shared->Uim->getState();
			model.realizeReport(stateCopy);

			coord.setLocked(stateCopy, false);

			int nPoints = shared->RequestedNumPlotPoints;
			double start = coord.getRangeMin();
			double end = coord.getRangeMax();
			double step = (end - start) / (nPoints == 1 ? 1 : nPoints - 1);

			m_XValues.clear();
			m_XValues.reserve(nPoints);
			m_YValues.clear();
			m_YValues.reserve(nPoints);

			for (int i = 0; i < nPoints; ++i)
			{
				double xVal = start + (i * step);

				coord.setValue(stateCopy, xVal);
				model.assemble(stateCopy);
				model.equilibrateMuscles(stateCopy);
				model.realizeReport(stateCopy);

				double yVald = shared->RequestedMuscleOutput(stateCopy, muscle, coord);
				float yVal = static_cast<float>(yVald);

				m_XValues.push_back(osc::ConvertCoordValueToDisplayValue(coord, xVal));
				m_YValues.push_back(yVal);
			}

			m_LastPlotModelVersion = shared->Uim->getModelVersion();
			m_LastPlotStateVersion = shared->Uim->getStateVersion();
			m_ActiveMuscleOutput = shared->RequestedMuscleOutput;
			m_NumPlotPointsPlotted = shared->RequestedNumPlotPoints;
		}

		std::string computePlotTitle(OpenSim::Coordinate const& c)
		{
			std::stringstream ss;
			appendYAxisName(ss);
			ss << " vs ";
			appendXAxisName(c, ss);
			return std::move(ss).str();
		}

		void appendYAxisName(std::stringstream& ss)
		{
			ss << m_ActiveMuscleOutput.getName();
		}

		void appendXAxisName(OpenSim::Coordinate const& c, std::stringstream& ss)
		{
			ss << c.getName();
		}

		std::string computePlotYAxisTitle()
		{
			std::stringstream ss;
			appendYAxisName(ss);
			ss << " [" << m_ActiveMuscleOutput.getUnits() << ']';
			return std::move(ss).str();
		}

		std::string computePlotXAxisTitle(OpenSim::Coordinate const& c)
		{
			std::stringstream ss;
			appendXAxisName(c, ss);
			ss << " value [" << osc::GetCoordDisplayValueUnitsString(c) << ']';
			return std::move(ss).str();
		}

		int m_NumPlotPointsEdited = shared->RequestedNumPlotPoints;
		int m_NumPlotPointsPlotted = 0;
		MuscleOutput m_ActiveMuscleOutput = shared->RequestedMuscleOutput;
		osc::UID m_LastPlotModelVersion;
		osc::UID m_LastPlotStateVersion;
		std::vector<float> m_XValues;
		std::vector<float> m_YValues;
	};

	// state in which a user is being prompted to select a coordinate in the model
	class PickCoordinateState final : public MusclePlotState {
	public:
		explicit PickCoordinateState(SharedStateData* shared_) : MusclePlotState{std::move(shared_)}
		{
			// this is what this state is populating
			osc::Clear(shared->RequestedCoordinateComponentPath);
		}

		std::unique_ptr<MusclePlotState> draw() override
		{
			std::unique_ptr<MusclePlotState> rv;

			std::vector<OpenSim::Coordinate const*> coordinates;
			for (OpenSim::Coordinate const& coord : shared->Uim->getModel().getComponentList<OpenSim::Coordinate>())
			{
				coordinates.push_back(&coord);
			}
			osc::Sort(coordinates, SortByComponentName);

			ImGui::Text("select coordinate:");

			ImGui::BeginChild("MomentArmPlotCoordinateSelection");
			for (OpenSim::Coordinate const* coord : coordinates)
			{
				if (ImGui::Selectable(coord->getName().c_str()))
				{
					shared->RequestedCoordinateComponentPath = coord->getAbsolutePath();
					rv = std::make_unique<ShowingPlotState>(shared);
				}
			}
			ImGui::EndChild();

			return rv;
		}
	};

	// state in which a user is being prompted to select a muscle in the model
	class PickMuscleState final : public MusclePlotState {
	public:
		explicit PickMuscleState(SharedStateData* shared_) : MusclePlotState{std::move(shared_)}
		{
			// this is what this state is populating
			osc::Clear(shared->RequestedMuscleComponentPath);
		}

		std::unique_ptr<MusclePlotState> draw() override
		{
			std::unique_ptr<MusclePlotState> rv;

			std::vector<OpenSim::Muscle const*> muscles;
			for (OpenSim::Muscle const& musc : shared->Uim->getModel().getComponentList<OpenSim::Muscle>())
			{
				muscles.push_back(&musc);
			}
			osc::Sort(muscles, SortByComponentName);

			ImGui::Text("select muscle:");

			ImGui::BeginChild("MomentArmPlotMuscleSelection");
			for (OpenSim::Muscle const* musc : muscles)
			{
				if (ImGui::Selectable(musc->getName().c_str()))
				{
					shared->RequestedMuscleComponentPath = musc->getAbsolutePath();
					rv = std::make_unique<PickCoordinateState>(shared);
				}
			}
			ImGui::EndChild();

			return rv;
		}
	};

	std::unique_ptr<MusclePlotState> CreateChooseCoordinateState(SharedStateData* shared)
	{
		return std::make_unique<PickCoordinateState>(std::move(shared));
	}

	std::unique_ptr<MusclePlotState> CreateChooseMuscleState(SharedStateData* shared)
	{
		return std::make_unique<PickMuscleState>(std::move(shared));
	}
}

// private IMPL for the muscle plot (effectively, a state machine host)
class osc::ModelMusclePlotPanel::Impl final {
public:
	Impl(std::shared_ptr<UndoableModelStatePair> uim, std::string_view panelName) :
		m_SharedData{std::move(uim)},
		m_ActiveState{std::make_unique<PickMuscleState>(&m_SharedData)},
		m_PanelName{std::move(panelName)}		
	{
	}

	Impl(std::shared_ptr<UndoableModelStatePair> uim,
         std::string_view panelName,
		 OpenSim::ComponentPath const& coordPath,
		 OpenSim::ComponentPath const& musclePath) :
		m_SharedData{std::move(uim), coordPath, musclePath},
		m_ActiveState{std::make_unique<ShowingPlotState>(&m_SharedData)},
		m_PanelName{std::move(panelName)}
	{
	}

	std::string const& getName() const
	{
		return m_PanelName;
	}

	bool isOpen() const
	{
		return m_IsOpen;
	}

	void open()
	{
		m_IsOpen = true;
	}

	void close()
	{
		m_IsOpen = false;
	}

	void draw()
	{
		if (m_IsOpen)
		{
			bool isOpen = m_IsOpen;
			if (ImGui::Begin(m_PanelName.c_str(), &isOpen))
			{
				if (auto maybeNextState = m_ActiveState->draw())
				{
					m_ActiveState = std::move(maybeNextState);
				}
				m_IsOpen = isOpen;
			}
			ImGui::End();

			if (isOpen != m_IsOpen)
			{
				m_IsOpen = isOpen;
			}
		}
	}

private:
	// data that's shared between all states
	SharedStateData m_SharedData;

	// currently active state (this class controls a state machine)
	std::unique_ptr<MusclePlotState> m_ActiveState;

	// name of the panel, as shown in the UI (via ImGui::Begin)
	std::string m_PanelName;

	// if the panel is currently open or not
	bool m_IsOpen = true;
};


// public API (PIMPL)

osc::ModelMusclePlotPanel::ModelMusclePlotPanel(std::shared_ptr<UndoableModelStatePair> uim,
	                                            std::string_view panelName) :
	m_Impl{new Impl{std::move(uim), std::move(panelName)}}
{
}

osc::ModelMusclePlotPanel::ModelMusclePlotPanel(std::shared_ptr<UndoableModelStatePair> uim,
                                                std::string_view panelName,
                                                OpenSim::ComponentPath const& coordPath,
                                                OpenSim::ComponentPath const& musclePath) :
	m_Impl{new Impl{std::move(uim), std::move(panelName), coordPath, musclePath}}
{
}

osc::ModelMusclePlotPanel::ModelMusclePlotPanel(ModelMusclePlotPanel&& tmp) noexcept :
	m_Impl{std::exchange(tmp.m_Impl, nullptr)}
{
}

osc::ModelMusclePlotPanel& osc::ModelMusclePlotPanel::operator=(ModelMusclePlotPanel&& tmp) noexcept
{
	std::swap(m_Impl, tmp.m_Impl);
	return *this;
}

osc::ModelMusclePlotPanel::~ModelMusclePlotPanel() noexcept
{
	delete m_Impl;
}

std::string const& osc::ModelMusclePlotPanel::getName() const
{
	return m_Impl->getName();
}

bool osc::ModelMusclePlotPanel::isOpen() const
{
	return m_Impl->isOpen();
}

void osc::ModelMusclePlotPanel::open()
{
	m_Impl->open();
}

void osc::ModelMusclePlotPanel::close()
{
	m_Impl->close();
}

void osc::ModelMusclePlotPanel::draw()
{
	m_Impl->draw();
}