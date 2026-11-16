#include <gtest/gtest.h>
#include <vector>
#include "core/ProcessMemory.h"

using ProcessMemory::peakResidentMegabytes;
using ProcessMemory::peakResidentMegabytesFromProcStatus;

TEST(ProcessMemory, ReadsVmHwmFromTheStatusText)
{
    const auto megabytes = peakResidentMegabytesFromProcStatus(
        "Name:\tOOpenCal-Viewer\n"
        "VmPeak:\t 9999999 kB\n"
        "VmHWM:\t  204800 kB\n"
        "VmRSS:\t  100000 kB\n");

    ASSERT_TRUE(megabytes.has_value());
    EXPECT_DOUBLE_EQ(*megabytes, 200.0); // the value in the file is in kB
}

TEST(ProcessMemory, AcceptsTheKeyInTheFirstAndInTheLastLineAndAnyWhitespace)
{
    EXPECT_DOUBLE_EQ(*peakResidentMegabytesFromProcStatus("VmHWM: 1024 kB\nVmRSS: 1 kB\n"), 1.0);
    EXPECT_DOUBLE_EQ(*peakResidentMegabytesFromProcStatus("VmRSS: 1 kB\nVmHWM:\t2048 kB"), 2.0); // no trailing newline
    EXPECT_DOUBLE_EQ(*peakResidentMegabytesFromProcStatus("VmHWM:512kB\n"), 0.5);
    EXPECT_DOUBLE_EQ(*peakResidentMegabytesFromProcStatus("VmHWM: 0 kB\n"), 0.0);
}

TEST(ProcessMemory, MissingOrBrokenLineGivesNothing)
{
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("").has_value());
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("Name:\tx\nVmRSS:\t5 kB\n").has_value());
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("VmHWM:\n").has_value());
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("VmHWM: lots kB\n").has_value());
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("  VmHWM: 5 kB\n").has_value()); // the key starts the line
    EXPECT_FALSE(peakResidentMegabytesFromProcStatus("VmHWM: -5 kB\n").has_value());
}

#ifdef __linux__
TEST(ProcessMemory, PeakOfTheRunningProcessGrowsWithTheMemoryThatWasUsed)
{
    const auto before = peakResidentMegabytes();
    ASSERT_TRUE(before.has_value());
    EXPECT_GT(*before, 0.0);

    std::vector<char> block(64 * 1024 * 1024, 1); // touched, so it really is resident
    ASSERT_EQ(block.back(), 1);

    const auto during = peakResidentMegabytes();
    ASSERT_TRUE(during.has_value());
    EXPECT_GE(*during, *before + 60.0);
}
#else
TEST(ProcessMemory, ReportsNothingWhereTheSystemIsNotSupported)
{
    EXPECT_FALSE(peakResidentMegabytes().has_value());
}
#endif
