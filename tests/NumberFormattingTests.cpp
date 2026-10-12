#include <gtest/gtest.h>
#include "widgets/NumberFormatting.h"

using NumberFormatting::withoutZeroFraction;
using NumberFormatting::withoutZeroFractionsInList;

TEST(NumberFormatting, ZeroFractionIsDroppedWithEitherSeparator)
{
    EXPECT_EQ(withoutZeroFraction("1008,000000"), "1008");
    EXPECT_EQ(withoutZeroFraction("1008.000000"), "1008");
    EXPECT_EQ(withoutZeroFraction("0,000000"), "0");
    EXPECT_EQ(withoutZeroFraction("0.000000"), "0");
    EXPECT_EQ(withoutZeroFraction("1400.0"), "1400");
    EXPECT_EQ(withoutZeroFraction("-3,000000"), "-3");
    EXPECT_EQ(withoutZeroFraction("+7.00"), "+7");
}

TEST(NumberFormatting, NegativeZeroBecomesZero)
{
    EXPECT_EQ(withoutZeroFraction("-0,000000"), "0");
    EXPECT_EQ(withoutZeroFraction("-0.000000"), "0");
}

TEST(NumberFormatting, SignificantFractionIsKept)
{
    EXPECT_EQ(withoutZeroFraction("0,500000"), "0,500000");
    EXPECT_EQ(withoutZeroFraction("12.250000"), "12.250000");
    EXPECT_EQ(withoutZeroFraction("1008,000001"), "1008,000001");
    EXPECT_EQ(withoutZeroFraction("-0,000100"), "-0,000100");
    EXPECT_EQ(withoutZeroFraction("10.05"), "10.05");
}

TEST(NumberFormatting, IntegersAndNonNumbersAreUntouched)
{
    EXPECT_EQ(withoutZeroFraction("1008"), "1008");
    EXPECT_EQ(withoutZeroFraction("-5"), "-5");
    EXPECT_EQ(withoutZeroFraction(""), "");
    EXPECT_EQ(withoutZeroFraction("abc"), "abc");
    EXPECT_EQ(withoutZeroFraction("-"), "-");
    EXPECT_EQ(withoutZeroFraction("1,"), "1,");
    EXPECT_EQ(withoutZeroFraction(",000000"), ",000000");
    EXPECT_EQ(withoutZeroFraction("1e+03"), "1e+03");
    EXPECT_EQ(withoutZeroFraction("1.0e+03"), "1.0e+03");
    EXPECT_EQ(withoutZeroFraction(" 1,000000"), " 1,000000");
    EXPECT_EQ(withoutZeroFraction("1,000000 m"), "1,000000 m");
}

// withoutZeroFraction() handles a single number only; lists go through withoutZeroFractionsInList().
TEST(NumberFormatting, SingleNumberFunctionLeavesListsUntouched)
{
    EXPECT_EQ(withoutZeroFraction("[1008,000000,0,000000]"), "[1008,000000,0,000000]");
    EXPECT_EQ(withoutZeroFraction("1,000000,2,000000"), "1,000000,2,000000");
}

TEST(NumberFormattingList, ZeroFractionsAreDroppedAndElementsSeparatedBySpace)
{
    // SciddicaT: sprintf("[%0.6f,%0.6f]", z, h)
    EXPECT_EQ(withoutZeroFractionsInList("[1008.000000,0.000000]"), "[1008, 0]");
    EXPECT_EQ(withoutZeroFractionsInList("[1008.000000,0.744905]"), "[1008, 0.744905]");
    EXPECT_EQ(withoutZeroFractionsInList("[1008.250000,0.500000]"), "[1008.250000, 0.500000]");
    EXPECT_EQ(withoutZeroFractionsInList("[-3.000000,-0.000000]"), "[-3, 0]");
    EXPECT_EQ(withoutZeroFractionsInList("[0.000000]"), "[0]");
    EXPECT_EQ(withoutZeroFractionsInList("[1,2,3]"), "[1, 2, 3]");
    EXPECT_EQ(withoutZeroFractionsInList("[7.5,0.000000,2]"), "[7.5, 0, 2]");
}

TEST(NumberFormattingList, AnythingElseIsReturnedUnchanged)
{
    EXPECT_EQ(withoutZeroFractionsInList(""), "");
    EXPECT_EQ(withoutZeroFractionsInList("[]"), "[]");
    EXPECT_EQ(withoutZeroFractionsInList("1008.000000"), "1008.000000");       // not a list
    EXPECT_EQ(withoutZeroFractionsInList("(1.0,2.0)"), "(1.0,2.0)");
    EXPECT_EQ(withoutZeroFractionsInList("[1.0,]"), "[1.0,]");
    EXPECT_EQ(withoutZeroFractionsInList("[,1.0]"), "[,1.0]");
    EXPECT_EQ(withoutZeroFractionsInList("[a,b]"), "[a,b]");
    EXPECT_EQ(withoutZeroFractionsInList("[1.0, 2.0]"), "[1.0, 2.0]");         // already spaced
    EXPECT_EQ(withoutZeroFractionsInList("[1.0e3,2]"), "[1.0e3,2]");
    EXPECT_EQ(withoutZeroFractionsInList("[1.,2]"), "[1.,2]");
}

// A list printed with a decimal comma cannot be split reliably. The "000000" elements that appear
// when "[1008,000000,0,000000]" is cut at every comma are rejected, so such text stays as it is.
TEST(NumberFormattingList, DecimalCommaListWithZeroFractionsIsRejected)
{
    EXPECT_EQ(withoutZeroFractionsInList("[1008,000000,0,000000]"), "[1008,000000,0,000000]");
}
