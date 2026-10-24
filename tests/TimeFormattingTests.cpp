#include <gtest/gtest.h>
#include <limits>
#include "widgets/TimeFormatting.h"

using TimeFormatting::plainSeconds;
using TimeFormatting::pretty;

TEST(TimeFormatting, PlainSecondsUsesShortestForm)
{
    EXPECT_EQ(plainSeconds(0), "0 s");
    EXPECT_EQ(plainSeconds(0.5), "0.5 s");
    EXPECT_EQ(plainSeconds(3600), "3600 s");
    EXPECT_EQ(plainSeconds(12345.678), "12345.678 s");
    EXPECT_EQ(plainSeconds(1000000), "1000000 s");   // no exponent notation
    EXPECT_EQ(plainSeconds(0.1 + 0.2), "0.3 s");     // accumulated floating point noise is rounded away
    EXPECT_EQ(plainSeconds(-2.5), "-2.5 s");
    EXPECT_EQ(plainSeconds(-0.0001), "0 s");         // no "-0 s"
}

TEST(TimeFormatting, PrettyBelowMinuteKeepsTenths)
{
    EXPECT_EQ(pretty(0), "0.0 s");
    EXPECT_EQ(pretty(0.5), "0.5 s");
    EXPECT_EQ(pretty(5), "5.0 s");
    EXPECT_EQ(pretty(59.9), "59.9 s");
}

TEST(TimeFormatting, PrettySecondsRollOverToMinutes)
{
    EXPECT_EQ(pretty(60), "1 min 0.0 s");
    EXPECT_EQ(pretty(65.5), "1 min 5.5 s");
    EXPECT_EQ(pretty(59.96), "1 min 0.0 s");        // rounds up through the minute boundary
    EXPECT_EQ(pretty(3599.9), "59 min 59.9 s");
}

TEST(TimeFormatting, PrettyMinutesRollOverToHours)
{
    EXPECT_EQ(pretty(3600), "1 h 0 min 0.0 s");
    EXPECT_EQ(pretty(3725.5), "1 h 2 min 5.5 s");
    EXPECT_EQ(pretty(3599.96), "1 h 0 min 0.0 s");
    EXPECT_EQ(pretty(86399.9), "23 h 59 min 59.9 s");
}

TEST(TimeFormatting, PrettyHoursRollOverToDaysAndTenthsDisappear)
{
    EXPECT_EQ(pretty(86400), "1 d 0 h 0 min 0 s");
    EXPECT_EQ(pretty(86399.96), "1 d 0 h 0 min 0 s");
    EXPECT_EQ(pretty(90061.4), "1 d 1 h 1 min 1 s");
    EXPECT_EQ(pretty(3 * 86400), "3 d 0 h 0 min 0 s");  // 3 days of simulated time, saved every hour
    EXPECT_EQ(pretty(3 * 86400 + 3600), "3 d 1 h 0 min 0 s");
}

TEST(TimeFormatting, PrettyHandlesUnusualValues)
{
    EXPECT_EQ(pretty(-65.5), "-1 min 5.5 s");
    EXPECT_EQ(pretty(-0.01), "0.0 s");
    EXPECT_EQ(pretty(std::numeric_limits<double>::quiet_NaN()), "n/a");
    EXPECT_EQ(pretty(std::numeric_limits<double>::infinity()), "n/a");
    EXPECT_EQ(plainSeconds(std::numeric_limits<double>::quiet_NaN()), "n/a");

    // Out of range for the integer unit split: falls back to plain seconds instead of overflowing
    const std::string huge = pretty(1e20);
    EXPECT_EQ(huge.find(" d "), std::string::npos);
    EXPECT_EQ(huge.substr(huge.size() - 2), " s");
}

TEST(TimeFormatting, FormatSelectsStyle)
{
    EXPECT_EQ(TimeFormatting::format(3725.5, true), "1 h 2 min 5.5 s");
    EXPECT_EQ(TimeFormatting::format(3725.5, false), "3725.5 s");
}
