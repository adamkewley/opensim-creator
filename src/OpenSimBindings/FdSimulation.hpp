#pragma once

#include "src/OpenSimBindings/BasicModelStatePair.hpp"
#include "src/OpenSimBindings/IntegratorMethod.hpp"
#include "src/OpenSimBindings/OutputExtractor.hpp"
#include "src/OpenSimBindings/ParamBlock.hpp"
#include "src/OpenSimBindings/SimulationClock.hpp"
#include "src/OpenSimBindings/SimulationStatus.hpp"

#include <functional>
#include <memory>

namespace OpenSim
{
    class Model;
}

namespace osc
{
    class SimulationReport;
}

namespace osc
{
    // returns outputs (e.g. auxiliary stuff like integration steps) that the
    // FdSimulator writes into the `SimulationReport`s it emits
    int GetNumFdSimulatorOutputExtractors();
    OutputExtractor GetFdSimulatorOutputExtractor(int);

    // simulation parameters
    struct FdParams final {

        // final time for the simulation
        SimulationClock::time_point FinalTime = SimulationClock::start() + SimulationClock::duration{10.0};

        // which integration method to use for the simulation
        IntegratorMethod IntegratorMethodUsed = IntegratorMethod::OpenSimManagerDefault;

        // the time interval, in simulation time, between report updates
        SimulationClock::duration ReportingInterval{1.0/100.0};  // 100 Hz

        // max number of *internal* steps that may be taken within a single call
        // to the integrator's stepTo or stepBy function
        //
        // this is mostly an internal concern, but can affect how regularly the
        // simulator reports updates (e.g. a lower number here *may* mean more
        // frequent per-significant-step updates)
        int IntegratorStepLimit = 20000;

        // minimum step, in time, that the integrator should attempt
        //
        // some integrators just ignore this
        SimulationClock::duration IntegratorMinimumStepSize{1.0e-8};

        // maximum step, in time, that an integrator can attempt
        //
        // e.g. even if the integrator *thinks* it can skip 10s of simulation time
        // it still *must* integrate to this size and return to the caller (i.e. the
        // simulator) to report the state at this maximum time
        SimulationClock::duration IntegratorMaximumStepSize{1.0};

        // accuracy of the integrator
        //
        // this only does something if the integrator is error-controlled and able
        // to improve accuracy (e.g. by taking many more steps)
        double IntegratorAccuracy = 1.0e-5;
    };

    // convert to a generic parameter block (for UI binding)
    ParamBlock ToParamBlock(FdParams const&);
    FdParams FromParamBlock(ParamBlock const&);

    // fd simulation that immediately starts running on a background thread
    class FdSimulation final {
    public:
        // immediately starts the simulation upon construction
        //
        // care: the callback is called *on the bg thread* - you should know how
        //       to handle it (e.g. mutexes) appropriately
        FdSimulation(BasicModelStatePair,
                     FdParams const& params,
                     std::function<void(SimulationReport)> onReportFromBgThread);

        FdSimulation(FdSimulation const&) = delete;
        FdSimulation(FdSimulation&&) noexcept;
        FdSimulation& operator=(FdSimulation const&) = delete;
        FdSimulation& operator=(FdSimulation&&) noexcept;
        ~FdSimulation() noexcept;

        SimulationStatus getStatus() const;
        void requestStop();  // asynchronous
        void stop();  // synchronous (blocks until it stops)
        FdParams const& params() const;

        class Impl;
    private:
        std::unique_ptr<Impl> m_Impl;
    };
}
