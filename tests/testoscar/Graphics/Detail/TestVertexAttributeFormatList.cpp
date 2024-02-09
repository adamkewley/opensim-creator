#include <oscar/Graphics/Detail/VertexAttributeFormatList.h>

#include <gtest/gtest.h>
#include <oscar/Graphics/VertexAttributeFormat.h>
#include <oscar/Graphics/Detail/VertexAttributeFormatTraits.h>
#include <oscar/Utils/EnumHelpers.h>
#include <oscar/Utils/NonTypelist.h>

#include <array>

using osc::detail::VertexAttributeFormatList;
using osc::detail::VertexAttributeFormatTraits;
using osc::NonTypelist;
using osc::NonTypelistSizeV;
using osc::NumOptions;
using osc::VertexAttributeFormat;

namespace
{
    template<VertexAttributeFormat... Formats>
    constexpr void InstantiateTraits(NonTypelist<VertexAttributeFormat, Formats...>)
    {
        [[maybe_unused]] auto a = std::to_array({VertexAttributeFormatTraits<Formats>::num_components...});
    }
}

TEST(VertexAttributeFormatList, HasAnEntryForEachVertexAttributeFormat)
{
    static_assert(NumOptions<VertexAttributeFormat>() == NonTypelistSizeV<VertexAttributeFormatList>);
}

TEST(VertexAttributeFormatList, EveryEntryInTheListHasAnAssociatedTraitsObject)
{
    InstantiateTraits(VertexAttributeFormatList{});
}
