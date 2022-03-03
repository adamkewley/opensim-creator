#pragma once

#include "src/OpenSimBindings/VirtualOutput.hpp"
#include "src/OpenSimBindings/Output.hpp"
#include "src/Utils/UID.hpp"

#include <optional>
#include <string>
#include <string_view>


namespace OpenSim
{
    class Component;
}

namespace osc
{
    class SimulationReport;
}

namespace SimTK
{
    class Integrator;
}


namespace osc
{
    class IntegratorOutput final : public VirtualOutput {
    public:
        using ExtractorFn = float (*)(SimTK::Integrator const&);

        IntegratorOutput(std::string_view name,
                         std::string_view description,
                         ExtractorFn extractor);

        UID getID() const override;
        OutputType getOutputType() const override;
        std::string const& getName() const override;
        std::string const& getDescription() const override;
        bool producesNumericValues() const override;
        std::optional<float> getNumericValue(OpenSim::Component const&, SimulationReport const&) const override;
        std::optional<std::string> getStringValue(OpenSim::Component const&, SimulationReport const&) const override;
        ExtractorFn getExtractorFunction() const;

    private:
        UID m_ID;
        std::string m_Name;
        std::string m_Description;
        ExtractorFn m_Extractor;
    };

    int GetNumIntegratorOutputs();
    IntegratorOutput const& GetIntegratorOutput(int idx);
    Output GetIntegratorOutputDynamic(int idx);
}
