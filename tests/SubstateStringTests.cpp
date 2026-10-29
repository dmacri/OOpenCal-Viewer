#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "visualiser/SettingParameter.h"
#include "visualiser/SubstateString.h"

namespace
{
constexpr double notSet = std::numeric_limits<double>::quiet_NaN();

SubstateInfo makeInfo(const std::string& name)
{
    SubstateInfo info;
    info.name = name;
    return info;
}

/// What the Viewer reads from the entry (the real parser, not a copy of it)
SubstateInfo readBack(const std::string& entry)
{
    SettingParameter sp;
    sp.substates = entry;
    const auto parsed = sp.parseSubstates();
    EXPECT_EQ(parsed.size(), 1u) << entry;
    return parsed.empty() ? SubstateInfo{} : parsed.begin()->second;
}
} // namespace

// ---------------------------------------------------------------- formatEntry()

TEST(SubstateStringFormat, OnlyName)
{
    EXPECT_EQ(SubstateString::formatEntry(makeInfo("psi")), "(psi,%f)");
}

TEST(SubstateStringFormat, MinMax)
{
    auto info = makeInfo("psi");
    info.minValue = 0.0;
    info.maxValue = 0.1;
    EXPECT_EQ(SubstateString::formatEntry(info), "(psi,%f,0,0.1)");
}

TEST(SubstateStringFormat, MinMaxNoValue)
{
    auto info = makeInfo("psi");
    info.minValue = 0.0;
    info.maxValue = 0.1;
    info.noValue = -9999.0;
    info.noValueEnabled = true;
    EXPECT_EQ(SubstateString::formatEntry(info), "(psi,%f,0,0.1,-9999)");
}

TEST(SubstateStringFormat, NoValueSwitchedOffIsNotWritten)
{
    auto info = makeInfo("psi");
    info.minValue = 0.0;
    info.maxValue = 1.0;
    info.noValue = -9999.0;
    info.noValueEnabled = false;
    EXPECT_EQ(SubstateString::formatEntry(info), "(psi,%f,0,1)");
}

TEST(SubstateStringFormat, NoValueOnlyUsesThreeParameters)
{
    auto info = makeInfo("tmp");
    info.noValue = -1.0;
    info.noValueEnabled = true;
    EXPECT_EQ(SubstateString::formatEntry(info), "(tmp,%f,-1)");
}

TEST(SubstateStringFormat, Colors)
{
    auto info = makeInfo("tmp");
    info.minValue = 1.0;
    info.maxValue = 100.0;
    info.noValue = -1.0;
    info.noValueEnabled = true;
    info.minColor = "#000011";
    info.maxColor = "#0011ff";
    EXPECT_EQ(SubstateString::formatEntry(info), "(tmp,%f,1,100,-1,#000011,#0011ff)");
}

TEST(SubstateStringFormat, CustomFormatIsKept)
{
    auto info = makeInfo("z");
    info.format = "%.2f";
    EXPECT_EQ(SubstateString::formatEntry(info), "(z,%.2f)");
}

TEST(SubstateStringFormat, EntriesAreJoinedWithCommas)
{
    auto a = makeInfo("a");
    auto b = makeInfo("b");
    b.minValue = 1.0;
    b.maxValue = 2.0;
    EXPECT_EQ(SubstateString::formatEntries({a, b}), "(a,%f),(b,%f,1,2)");
    EXPECT_EQ(SubstateString::formatEntries({}), "");
}

// What is written must be read back by the real parser as the same substate
TEST(SubstateStringFormat, EveryCombinationIsReadBackByTheParser)
{
    const double minValues[] = {notSet, -2.5};
    const double maxValues[] = {notSet, 7.25};
    const bool noValueOptions[] = {false, true};
    const std::string minColors[] = {"", "#102030"};
    const std::string maxColors[] = {"", "#a0b0c0"};

    for (double minValue : minValues)
        for (double maxValue : maxValues)
            for (bool noValueOn : noValueOptions)
                for (const auto& minColor : minColors)
                    for (const auto& maxColor : maxColors)
                    {
                        auto info = makeInfo("psi");
                        info.minValue = minValue;
                        info.maxValue = maxValue;
                        info.noValue = noValueOn ? -9999.0 : notSet;
                        info.noValueEnabled = noValueOn;
                        info.minColor = minColor;
                        info.maxColor = maxColor;

                        const std::string entry = SubstateString::formatEntry(info);
                        const SubstateInfo back = readBack(entry);

                        SCOPED_TRACE(entry);
                        EXPECT_EQ(back.name, "psi");
                        EXPECT_EQ(std::isnan(back.minValue), std::isnan(minValue));
                        EXPECT_EQ(std::isnan(back.maxValue), std::isnan(maxValue));
                        if (! std::isnan(minValue))
                            EXPECT_EQ(back.minValue, minValue);
                        if (! std::isnan(maxValue))
                            EXPECT_EQ(back.maxValue, maxValue);
                        EXPECT_EQ(back.noValueEnabled, noValueOn);
                        if (noValueOn)
                            EXPECT_EQ(back.noValue, -9999.0);
                        EXPECT_EQ(back.minColor, minColor);
                        EXPECT_EQ(back.maxColor, maxColor);
                    }
}

TEST(SubstateStringFormat, SeveralEntriesAreReadBackInFull)
{
    auto psi = makeInfo("psi");
    psi.minValue = 0.0;
    psi.maxValue = 0.1;
    psi.noValue = -9999.0;
    psi.noValueEnabled = true;
    auto teta = makeInfo("teta");
    teta.minValue = 0.0;
    teta.maxValue = 0.5;

    SettingParameter sp;
    sp.substates = SubstateString::formatEntries({psi, teta});
    const auto parsed = sp.parseSubstates();
    ASSERT_EQ(parsed.size(), 2u) << sp.substates;
    EXPECT_EQ(parsed.at("psi").noValue, -9999.0);
    EXPECT_EQ(parsed.at("teta").maxValue, 0.5);
}

// ---------------------------------------------------------------- numbers

TEST(SubstateStringNumbers, TextIsShortAndExact)
{
    EXPECT_EQ(SubstateString::numberToText(0.1), "0.1");
    EXPECT_EQ(SubstateString::numberToText(-9999.0), "-9999");
    EXPECT_EQ(SubstateString::numberToText(0.0), "0");
    EXPECT_EQ(SubstateString::numberToText(1.5e-7), "1.5e-07");
    EXPECT_EQ(SubstateString::numberToText(notSet), "nan");
    for (double value : {0.1, 1.0 / 3.0, 123456.789, -2.5e-300})
        EXPECT_EQ(*SubstateString::parseNumber(SubstateString::numberToText(value)), value);
}

TEST(SubstateStringNumbers, ParseNumberTakesOnlyWholeNumbers)
{
    EXPECT_EQ(SubstateString::parseNumber("0.140319"), 0.140319);
    EXPECT_EQ(SubstateString::parseNumber("  -9999.000000 "), -9999.0);
    EXPECT_EQ(SubstateString::parseNumber("+2"), 2.0);
    EXPECT_EQ(SubstateString::parseNumber("1e3"), 1000.0);
    EXPECT_TRUE(std::isnan(*SubstateString::parseNumber("nan")));

    EXPECT_FALSE(SubstateString::parseNumber(""));
    EXPECT_FALSE(SubstateString::parseNumber("   "));
    EXPECT_FALSE(SubstateString::parseNumber("abc"));
    EXPECT_FALSE(SubstateString::parseNumber("1.5abc")); // std::stod() would take 1.5
    EXPECT_FALSE(SubstateString::parseNumber("[0.012000,0.350000,0.012000,10.000000]"));
    EXPECT_FALSE(SubstateString::parseNumber("0,5")); // a decimal comma: not what a model writes
}

// ---------------------------------------------------------------- validateName()

TEST(SubstateStringName, GoodNamesPass)
{
    EXPECT_EQ(SubstateString::validateName("psi", {}), "");
    EXPECT_EQ(SubstateString::validateName("teta_Subsurface", {"psi"}), "");
    EXPECT_EQ(SubstateString::validateName("  psi ", {"h"}), ""); // the dialog trims
}

TEST(SubstateStringName, BadNamesAreExplained)
{
    EXPECT_NE(SubstateString::validateName("", {}), "");
    EXPECT_NE(SubstateString::validateName("   ", {}), "");
    EXPECT_NE(SubstateString::validateName("a b", {}), "");
    EXPECT_NE(SubstateString::validateName("a,b", {}), "");
    EXPECT_NE(SubstateString::validateName("(a)", {}), "");
    EXPECT_NE(SubstateString::validateName("a=b", {}), "");
}

TEST(SubstateStringName, DuplicateIsRejected)
{
    const std::string message = SubstateString::validateName("psi", {"h", "psi"});
    EXPECT_NE(message.find("already exists"), std::string::npos) << message;
}

// ---------------------------------------------------------------- summarizeValues()

TEST(SubstateStringSummary, AllNumbers)
{
    const std::vector<std::string> cells = {"0.5", "-1", "3.25", "-9999", "nan"};
    const auto summary = SubstateString::summarizeValues(
        cells.size(), 1, [&](std::size_t i) { return cells[i]; }, -9999.0);

    EXPECT_EQ(summary.cellsChecked, 5u);
    EXPECT_EQ(summary.numeric, 5u);
    EXPECT_EQ(summary.notNumeric, 0u);
    EXPECT_EQ(summary.usedForRange, 3u); // -9999 is the no-value, nan is not a value
    EXPECT_EQ(summary.minValue, -1.0);
    EXPECT_EQ(summary.maxValue, 3.25);
    EXPECT_EQ(SubstateString::describeProblem(summary, "psi"), "");
}

TEST(SubstateStringSummary, NoValueOffTakesEveryNumber)
{
    const std::vector<std::string> cells = {"0.5", "-9999"};
    const auto summary = SubstateString::summarizeValues(cells.size(), 1, [&](std::size_t i) { return cells[i]; });
    EXPECT_EQ(summary.minValue, -9999.0);
    EXPECT_EQ(summary.maxValue, 0.5);
}

// This is the HydroCAL3D case: the cell returns the whole "[h,teta,psi,quota]" whatever the name is
TEST(SubstateStringSummary, ModelWhichReturnsTheWholeCellIsRejected)
{
    const auto summary = SubstateString::summarizeValues(
        4, 1, [](std::size_t) { return std::string("[0.012000,0.350000,0.012000,10.000000]"); });

    EXPECT_EQ(summary.numeric, 0u);
    EXPECT_EQ(summary.notNumeric, 4u);
    EXPECT_EQ(summary.firstNotNumericText, "[0.012000,0.350000,0.012000,10.000000]");

    const std::string problem = SubstateString::describeProblem(summary, "psi");
    EXPECT_NE(problem.find("stringEncoding(\"psi\")"), std::string::npos) << problem;
    EXPECT_NE(problem.find("4 of 4"), std::string::npos) << problem;
    EXPECT_NE(problem.find("[0.012000"), std::string::npos) << problem;
}

TEST(SubstateStringSummary, OneBadCellIsEnoughToReject)
{
    const std::vector<std::string> cells = {"1", "2", "oops", "4"};
    const auto summary = SubstateString::summarizeValues(cells.size(), 1, [&](std::size_t i) { return cells[i]; });
    EXPECT_EQ(summary.notNumeric, 1u);
    EXPECT_NE(SubstateString::describeProblem(summary, "x"), "");
}

TEST(SubstateStringSummary, StrideAsksEveryNthCell)
{
    std::vector<std::size_t> asked;
    const auto summary = SubstateString::summarizeValues(
        10, 4, [&](std::size_t i) { asked.push_back(i); return std::string("1"); });
    EXPECT_EQ(asked, (std::vector<std::size_t>{0, 4, 8}));
    EXPECT_EQ(summary.cellsChecked, 3u);

    asked.clear();
    SubstateString::summarizeValues(3, 0, [&](std::size_t i) { asked.push_back(i); return std::string("1"); }); // 0 means 1
    EXPECT_EQ(asked.size(), 3u);
}

TEST(SubstateStringSummary, NoCellsIsAProblem)
{
    const auto summary = SubstateString::summarizeValues(0, 1, [](std::size_t) { return std::string("1"); });
    EXPECT_NE(SubstateString::describeProblem(summary, "psi"), "");
}
