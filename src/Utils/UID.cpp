#include "UID.hpp"

#include <iostream>

std::atomic<int64_t> osc::UID::g_NextID = 0;

std::ostream& osc::operator<<(std::ostream& o, UID const& id)
{
    return o << id.get();
}
