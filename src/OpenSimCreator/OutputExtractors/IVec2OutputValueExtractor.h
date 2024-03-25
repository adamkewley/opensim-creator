#pragma once

#include <OpenSimCreator/Documents/Simulation/SimulationReport.h>

#include <oscar/Maths/Vec2.h>

#include <functional>
#include <span>

namespace OpenSim { class Component; }

namespace osc
{
    // an output value extractor that extracts floats
    class IVec2OutputValueExtractor {
    protected:
        IVec2OutputValueExtractor() = default;
        IVec2OutputValueExtractor(IVec2OutputValueExtractor const&) = default;
        IVec2OutputValueExtractor(IVec2OutputValueExtractor&&) noexcept = default;
        IVec2OutputValueExtractor& operator=(IVec2OutputValueExtractor const&) = default;
        IVec2OutputValueExtractor& operator=(IVec2OutputValueExtractor&&) noexcept = default;
    public:
        virtual ~IVec2OutputValueExtractor() noexcept = default;

        void extractVec2s(
            OpenSim::Component const& component,
            std::span<SimulationReport const> reports,
            std::function<void(Vec2)> const& consumer) const
        {
            implExtractFloats(component, reports, consumer);
        }

    private:
        virtual void implExtractFloats(
            OpenSim::Component const&,
            std::span<SimulationReport const>,
            std::function<void(Vec2)> const& consumer
        ) const = 0;
    };
}
