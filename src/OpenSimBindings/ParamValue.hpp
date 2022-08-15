#pragma once

#include "src/OpenSimBindings/IntegratorMethod.hpp"

#include <variant>

namespace osc
{
    using ParamValue = std::variant<double, int, IntegratorMethod>;
}