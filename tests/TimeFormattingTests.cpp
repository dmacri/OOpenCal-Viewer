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

TEST(TimeFormatting, PlainKeepsTheUnitOfTheModel)
{
    EXPECT_EQ(TimeFormatting::plain(2500, TimeUnit::Milliseconds), "2500 ms");
    EXPECT_EQ(TimeFormatting::plain(0, TimeUnit::Milliseconds), "0 ms");
    EXPECT_EQ(TimeFormatting::plain(12.5, TimeUnit::Milliseconds), "12.5 ms");
    EXPECT_EQ(TimeFormatting::plain(5.5, TimeUnit::Seconds), "5.5 s");
    EXPECT_EQ(TimeFormatting::format(19995, TimeUnit::Milliseconds, false), "19995 ms");
}

TEST(TimeFormatting, PrettyOfMillisecondsKeepsMillisecondResolutionUntilFirstDay)
{
    using TimeFormatting::format;
    constexpr auto ms = TimeUnit::Milliseconds;

    EXPECT_EQ(format(0, ms, true), "0.000 s");
    EXPECT_EQ(format(12, ms, true), "0.012 s");
    EXPECT_EQ(format(500, ms, true), "0.500 s");
    EXPECT_EQ(format(2500, ms, true), "2.500 s");
    EXPECT_EQ(format(19995, ms, true), "19.995 s");
    EXPECT_EQ(format(65500, ms, true), "1 min 5.500 s");
    EXPECT_EQ(format(59999.6, ms, true), "1 min 0.000 s");           // rounds through the minute boundary
    EXPECT_EQ(format(3725500, ms, true), "1 h 2 min 5.500 s");
    EXPECT_EQ(format(86399999, ms, true), "23 h 59 min 59.999 s");
    EXPECT_EQ(format(86400000, ms, true), "1 d 0 h 0 min 0 s");      // fractions disappear from the first day on
    EXPECT_EQ(format(90061400, ms, true), "1 d 1 h 1 min 1 s");
    EXPECT_EQ(format(-65500, ms, true), "-1 min 5.500 s");
}

TEST(TimeFormatting, PrettyOfSecondsStillShowsTenths)
{
    EXPECT_EQ(TimeFormatting::format(2500, TimeUnit::Seconds, true), "41 min 40.0 s");
    EXPECT_EQ(TimeFormatting::pretty(5.55, 3), "5.550 s");
    EXPECT_EQ(TimeFormatting::pretty(5.5, 0), "6 s");
    EXPECT_EQ(TimeFormatting::pretty(65.4, 0), "1 min 5 s");
}

TEST(TimeUnit, ParsesNamesCaseInsensitively)
{
    EXPECT_EQ(parseTimeUnit("ms"), TimeUnit::Milliseconds);
    EXPECT_EQ(parseTimeUnit(" MS "), TimeUnit::Milliseconds);
    EXPECT_EQ(parseTimeUnit("s"), TimeUnit::Seconds);
    EXPECT_FALSE(parseTimeUnit("").has_value());
    EXPECT_FALSE(parseTimeUnit("min").has_value());
    EXPECT_DOUBLE_EQ(timeUnitInSeconds(TimeUnit::Milliseconds), 0.001);
}
