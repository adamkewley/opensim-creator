#pragma once

#include <OpenSimCreator/Simulation/SimulationClock.hpp>
#include <OpenSimCreator/Simulation/SimulationReport.hpp>

#include <SDL_events.h>

#include <memory>
#include <optional>
#include <string_view>

namespace osc { class Simulation; }
namespace osc { class SimulatorUIAPI; }

namespace osc
{
    class SimulationScrubber final {
    public:
        SimulationScrubber(
            std::string_view label,
            SimulatorUIAPI*,
            std::shared_ptr<Simulation const>
        );
        SimulationScrubber(SimulationScrubber const&) = delete;
        SimulationScrubber(SimulationScrubber&&) noexcept;
        SimulationScrubber& operator=(SimulationScrubber const&) = delete;
        SimulationScrubber& operator=(SimulationScrubber&&) noexcept;
        ~SimulationScrubber() noexcept;

        void onDraw();

    private:
        class Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}