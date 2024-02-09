#include <oscar/Graphics/Detail/VertexAttributeList.h>

#include <gtest/gtest.h>
#include <oscar/Graphics/VertexAttribute.h>
#include <oscar/Graphics/Detail/VertexAttributeTraits.h>
#include <oscar/Utils/EnumHelpers.h>
#include <oscar/Utils/NonTypelist.h>

#include <array>

using osc::detail::VertexAttributeList;
using osc::detail::VertexAttributeTraits;
using osc::NonTypelist;
using osc::NonTypelistSizeV;
using osc::NumOptions;
using osc::VertexAttribute;

namespace
{
    template<VertexAttribute... Attributes>
    constexpr void InstantiateTraits(NonTypelist<VertexAttribute, Attributes...>)
    {
        [[maybe_unused]] auto a = std::to_array({VertexAttributeTraits<Attributes>::default_format...});
    }
}

TEST(VertexAttributeList, HasAnEntryForEachVertexAttribute)
{
    static_assert(NumOptions<VertexAttribute>() == NonTypelistSizeV<VertexAttributeList>);
}

TEST(VertexAttributeList, EveryEntryInTheListHasAnAssociatedTraitsObject)
{
    InstantiateTraits(VertexAttributeList{});
}
