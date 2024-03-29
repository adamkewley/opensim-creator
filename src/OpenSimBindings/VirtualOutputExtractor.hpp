#pragma once

#include <nonstd/span.hpp>

#include <string>

namespace OpenSim
{
    class Component;
}

namespace osc
{
    class SimulationReport;
}

namespace osc
{
    // indicates the datatype that the output extractor emits
    enum class OutputType {
        Float = 0,
        String,
        TOTAL,
    };

    // interface for something that can extract data from simulation reports
    //
    // assumed to be an immutable type (important, because output extractors
    // might be shared between simulations, threads, etc.) that merely extracts
    // data from simulation reports
    class VirtualOutputExtractor {
    public:
        virtual ~VirtualOutputExtractor() noexcept = default;

        virtual std::string const& getName() const = 0;
        virtual std::string const& getDescription() const = 0;

        virtual OutputType getOutputType() const = 0;

        virtual float getValueFloat(OpenSim::Component const&, SimulationReport const&) const = 0;

        virtual void getValuesFloat(OpenSim::Component const&,
                                    nonstd::span<SimulationReport const>,
                                    nonstd::span<float> overwriteOut) const = 0;

        virtual std::string getValueString(OpenSim::Component const&,
                                           SimulationReport const&) const = 0;
    };
}
