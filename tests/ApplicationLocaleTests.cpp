#include <gtest/gtest.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/ApplicationLocale.h"

namespace appLocale = viz::appLocale;

namespace
{
/// A locale with a decimal comma which is installed on this machine, empty when there is none
std::string installedCommaLocale()
{
    for (const char* name : {"pl_PL.UTF-8", "it_IT.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "pl_PL.utf8", "it_IT.utf8", "de_DE.utf8"})
    {
        if (appLocale::decimalPointOf(name) == ',')
            return name;
    }
    return {};
}
} // namespace

class ApplicationLocaleTests : public ::testing::Test
{
protected:
    void TearDown() override { appLocale::apply(appLocale::defaultLocaleName); } // the other tests expect "C"
};

TEST_F(ApplicationLocaleTests, DecimalPointOfCLocaleIsDot)
{
    EXPECT_EQ(appLocale::decimalPointOf("C"), '.');
    EXPECT_EQ(appLocale::decimalPointOf(appLocale::defaultLocaleName), '.');
}

TEST_F(ApplicationLocaleTests, DecimalPointOfUnknownLocaleIsNullopt)
{
    EXPECT_EQ(appLocale::decimalPointOf("no_SUCH.locale-xyz"), std::nullopt);
}

TEST_F(ApplicationLocaleTests, ApplyUnknownLocaleFailsAndKeepsTheCurrentOne)
{
    ASSERT_TRUE(appLocale::apply("C"));

    std::string error;
    EXPECT_FALSE(appLocale::apply("no_SUCH.locale-xyz", &error));
    EXPECT_NE(error.find("no_SUCH.locale-xyz"), std::string::npos) << error;
    EXPECT_EQ(appLocale::activeName(), "C");
}

TEST_F(ApplicationLocaleTests, ApplyDefaultLocaleSucceeds)
{
    EXPECT_TRUE(appLocale::apply(appLocale::defaultLocaleName));
    EXPECT_EQ(appLocale::activeName(), "C");
}

TEST_F(ApplicationLocaleTests, ReadWithDecimalDot)
{
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("1008.5", '.'), 1008.5);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("1008,5", '.'), 1008.0); // the comma ends the number
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("  +3.25abc", '.'), 3.25);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("-1e3", '.'), -1000.0);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("abc", '.'), 0.0);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("", '.'), 0.0);
}

TEST_F(ApplicationLocaleTests, ReadWithDecimalComma)
{
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("1008,5", ','), 1008.5);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("1008.5", ','), 1008.0); // the dot ends the number: the digits after it are lost
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("1.234,5", ','), 1.0);
    EXPECT_DOUBLE_EQ(appLocale::readLikeStrtod("-0,25", ','), -0.25);
}

TEST_F(ApplicationLocaleTests, WriteReplacesTheDecimalPoint)
{
    EXPECT_EQ(appLocale::writeLikePrintf(1008.5, 1, '.'), "1008.5");
    EXPECT_EQ(appLocale::writeLikePrintf(1008.5, 1, ','), "1008,5");
    EXPECT_EQ(appLocale::writeLikePrintf(0.125, 3, ','), "0,125");
    EXPECT_EQ(appLocale::writeLikePrintf(-2.0, 2, ','), "-2,00");
}

// The emulation which the dialog shows to the user has to agree with what the C library really does
TEST_F(ApplicationLocaleTests, EmulationAgreesWithTheCLibraryInARealCommaLocale)
{
    const std::string name = installedCommaLocale();
    if (name.empty())
        GTEST_SKIP() << "no locale with a decimal comma is installed (e.g. sudo locale-gen pl_PL.UTF-8)";

    ASSERT_TRUE(appLocale::apply(name));
    EXPECT_EQ(appLocale::activeName(), name);

    for (const char* text : {"1008.5", "1008,5", "1.234,5", "  -0,25", "+7,5e1", "abc", "42"})
        EXPECT_DOUBLE_EQ(std::strtod(text, nullptr), appLocale::readLikeStrtod(text, ',')) << "text: " << text;

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f", 1008.5);
    EXPECT_EQ(std::string(buffer), appLocale::writeLikePrintf(1008.5, 1, ','));
    std::snprintf(buffer, sizeof(buffer), "%.3f", -0.125);
    EXPECT_EQ(std::string(buffer), appLocale::writeLikePrintf(-0.125, 3, ','));
}

TEST_F(ApplicationLocaleTests, EmulationAgreesWithTheCLibraryInTheCLocale)
{
    ASSERT_TRUE(appLocale::apply("C"));
    for (const char* text : {"1008.5", "1008,5", "  -0.25", "+7.5e1", "abc", "42"})
        EXPECT_DOUBLE_EQ(std::strtod(text, nullptr), appLocale::readLikeStrtod(text, '.')) << "text: " << text;
}
