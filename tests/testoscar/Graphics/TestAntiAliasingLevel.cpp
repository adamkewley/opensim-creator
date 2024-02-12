#include <oscar/Graphics/AntiAliasingLevel.h>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace osc;

TEST(AntiAliasingLevel, DefaultCtorIsEquivalentTo1XLevel)
{
    static_assert(AntiAliasingLevel{} == AntiAliasingLevel{1});
}

TEST(AntiAliasingLevel, CtorWithZeroClampsToOne)
{
    static_assert(AntiAliasingLevel{0} == AntiAliasingLevel{1});
}

TEST(AntiAliasingLevel, CtorWithBelowZeroClampsToOne)
{
    static_assert(AntiAliasingLevel{-1} == AntiAliasingLevel{1});
}

TEST(AntiAliasingLevel, InvalidValuesClampedToNextLowerCorrectValue)
{
    static_assert(AntiAliasingLevel{3} == AntiAliasingLevel{2});
    static_assert(AntiAliasingLevel{5} == AntiAliasingLevel{4});
    static_assert(AntiAliasingLevel{6} == AntiAliasingLevel{4});
    static_assert(AntiAliasingLevel{7} == AntiAliasingLevel{4});
    static_assert(AntiAliasingLevel{8} == AntiAliasingLevel{8});
    static_assert(AntiAliasingLevel{9} == AntiAliasingLevel{8});
    static_assert(AntiAliasingLevel{10} == AntiAliasingLevel{8});
    static_assert(AntiAliasingLevel{15} == AntiAliasingLevel{8});
    static_assert(AntiAliasingLevel{16} == AntiAliasingLevel{16});
    static_assert(AntiAliasingLevel{17} == AntiAliasingLevel{16});
    static_assert(AntiAliasingLevel{31} == AntiAliasingLevel{16});
    static_assert(AntiAliasingLevel{32} == AntiAliasingLevel{32});
    static_assert(AntiAliasingLevel{33} == AntiAliasingLevel{32});
}

TEST(AntiAliasingLevel, IncrementGoesToTheNextLogicalAntiAliasingLevel)
{
    static_assert(++AntiAliasingLevel{1} == AntiAliasingLevel{2});
    static_assert(++AntiAliasingLevel{2} == AntiAliasingLevel{4});
    static_assert(++AntiAliasingLevel{4} == AntiAliasingLevel{8});
    static_assert(++AntiAliasingLevel{8} == AntiAliasingLevel{16});
}

TEST(AntiAliasingLevel, LessThanWorksAsExpected)
{
    static_assert(AntiAliasingLevel{1} < AntiAliasingLevel{2});
    static_assert(AntiAliasingLevel{2} < AntiAliasingLevel{4});
    static_assert(AntiAliasingLevel{4} < AntiAliasingLevel{8});
    static_assert(AntiAliasingLevel{8} < AntiAliasingLevel{16});
    static_assert(AntiAliasingLevel{16} < AntiAliasingLevel{32});
    static_assert(AntiAliasingLevel{32} < AntiAliasingLevel{64});
}

TEST(AntiAliasingLevel, GetU32ReturnsExpectedValues)
{
    static_assert(AntiAliasingLevel{-1}.getU32() == 1u);
    static_assert(AntiAliasingLevel{1}.getU32() == 1u);
    static_assert(AntiAliasingLevel{2}.getU32() == 2u);
    static_assert(AntiAliasingLevel{3}.getU32() == 2u);
    static_assert(AntiAliasingLevel{4}.getU32() == 4u);
    static_assert(AntiAliasingLevel{8}.getU32() == 8u);
}

TEST(AntiAliasingLevel, CanStreamToOutput)
{
    auto toString = [](AntiAliasingLevel lvl) -> std::string
    {
        std::stringstream ss;
        ss << lvl;
        return std::move(ss).str();
    };

    ASSERT_EQ(toString(AntiAliasingLevel{1}), "1x");
    ASSERT_EQ(toString(AntiAliasingLevel{2}), "2x");
    ASSERT_EQ(toString(AntiAliasingLevel{4}), "4x");
    ASSERT_EQ(toString(AntiAliasingLevel{8}), "8x");
    ASSERT_EQ(toString(AntiAliasingLevel{16}), "16x");
    ASSERT_EQ(toString(AntiAliasingLevel{32}), "32x");
    ASSERT_EQ(toString(AntiAliasingLevel{64}), "64x");
}

TEST(AntiAliasingLevel, MinReturns1X)
{
    static_assert(AntiAliasingLevel::min() == AntiAliasingLevel{1});
}

TEST(AntiAliasingLevel, NoneReturns1X)
{
    static_assert(AntiAliasingLevel::none() == AntiAliasingLevel{1});
}
