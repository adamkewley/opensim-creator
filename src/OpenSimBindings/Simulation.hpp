#pragma once

#include "src/OpenSimBindings/SimulationClock.hpp"
#include "src/OpenSimBindings/SimulationStatus.hpp"
#include "src/OpenSimBindings/VirtualSimulation.hpp"
#include "src/OpenSimBindings/SimulationReport.hpp"
#include "src/Utils/UID.hpp"
#include "src/Utils/SynchronizedValue.hpp"

#include <nonstd/span.hpp>

#include <memory>
#include <optional>
#include <vector>
#include <string>

namespace osc
{
    class OutputExtractor;
    class ParamBlock;
}

namespace OpenSim
{
    class Model;
}

namespace SimTK
{
    class State;
}
namespace osc
{
    // a "value type" that acts as a container for a osc::VirtualSimulation
    class Simulation final {
    public:
        template<class ConcreteSimulation>
        Simulation(ConcreteSimulation&& simulation) :
            m_Simulation{std::make_unique<ConcreteSimulation>(std::forward<ConcreteSimulation>(simulation))}
        {
        }

        SynchronizedValueGuard<OpenSim::Model const> getModel() const { return m_Simulation->getModel(); }

        int getNumReports() { return m_Simulation->getNumReports(); }
        SimulationReport getSimulationReport(int reportIndex) { return m_Simulation->getSimulationReport(std::move(reportIndex)); }
        std::vector<SimulationReport> getAllSimulationReports() const { return m_Simulation->getAllSimulationReports(); }

        SimulationStatus getStatus() const { return m_Simulation->getStatus(); }
        SimulationClock::time_point getCurTime() { return m_Simulation->getCurTime(); }
        SimulationClock::time_point getStartTime() const { return m_Simulation->getStartTime(); }
        SimulationClock::time_point getEndTime() const { return m_Simulation->getEndTime(); }
        float getProgress() const { return m_Simulation->getProgress(); }
        ParamBlock const& getParams() const { return m_Simulation->getParams(); }
        nonstd::span<OutputExtractor const> getOutputs() const { return m_Simulation->getOutputExtractors(); }

        void requestStop() { m_Simulation->requestStop(); }
        void stop() { m_Simulation->stop(); }

        operator VirtualSimulation& () { return *m_Simulation; }
        operator VirtualSimulation const& () const { return *m_Simulation; }

    private:
        std::unique_ptr<VirtualSimulation> m_Simulation;
    };
}
