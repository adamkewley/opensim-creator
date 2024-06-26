#include <oscar/Utils/CStringView.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string_view>

using namespace osc;
using namespace osc::literals;

TEST(CStringView, WhenPassedNullCstringYieldsEmptyCStringView)
{
    char const* p = nullptr;
    ASSERT_TRUE(CStringView{p}.empty());
}

TEST(CStringView, WhenPassedNullCStringYieldsNonNullCStr)
{
    char const* p = nullptr;
    ASSERT_NE(CStringView{p}.c_str(), nullptr);
}

TEST(CStringView, WhenDefaultConstructedYieldsEmptyCStringView)
{
    ASSERT_TRUE(CStringView{}.empty());
}

TEST(CStringView, WhenDefaultConstructedYieldsNonNullCStr)
{
    ASSERT_NE(CStringView{}.c_str(), nullptr);
}

TEST(CStringView, WhenConstructedFromNullptrYieldsEmptyCStringView)
{
    ASSERT_TRUE(CStringView{nullptr}.empty());
}

TEST(CStringView, WhenConstructedFromNullptrYieldsNonNullCStr)
{
    ASSERT_NE(CStringView{nullptr}.c_str(), nullptr);
}

TEST(CStringView, ThreeWayComparisonBehavesIdenticallyToStringViewComparision)
{
    auto const svs = std::to_array<char const*>({ "x", "somestring", "somethingelse", "", "_i hope it works ;)" });
    auto sameThreeWayResultWithAllOtherElements = [&svs](char const* elCStr)
    {
        std::string_view sv{elCStr};
        CStringView csv{elCStr};
        for (char const* otherCStr : svs)
        {
            ASSERT_EQ(sv <=> std::string_view{otherCStr}, csv <=> CStringView{otherCStr});
        }
    };
    std::for_each(svs.begin(), svs.end(), sameThreeWayResultWithAllOtherElements);
}

TEST(CStringView, LiteralSuffixReturnsCStringView)
{
    static_assert(std::same_as<decltype("hello"_cs), CStringView>);
    ASSERT_EQ("hello"_cs, CStringView{"hello"});
}
