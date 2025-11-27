#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include "visualiser/SettingParameter.h"

/**
 * Test Suite: SettingParameter::parseSubstates()
 *
 * This test suite verifies the parsing of substate strings with various formats:
 * - Simple format: "h,z" or "s"
 * - Extended format with min/max: "(h,%f,1,100)" or "(z,%f,400,1400)"
 * - Extended format with no-value: "(tmp,%f,-1)" or "(tmp,%f,1,100,-1)"
 * - Extended format with colors: "(tmp,%f,1,100,-1,#000011,#0011ff)"
 * - Mixed formats: "h,(z,%f,1,100),s"
 */

// Helper function to check if a double is NaN
static bool isNaN(double value)
{
    return std::isnan(value);
}

// ============================================================================
// Test 1: Empty substates string
// ============================================================================
TEST(SettingParameterParseSubstates, EmptyString)
{
    SettingParameter sp;
    sp.substates = "";
    
    auto result = sp.parseSubstates();
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// Test 2: Simple format with single field
// ============================================================================
TEST(SettingParameterParseSubstates, SimpleFormatSingleField)
{
    SettingParameter sp;
    sp.substates = "h";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("h"));
    EXPECT_EQ(result["h"].name, "h");
    EXPECT_TRUE(isNaN(result["h"].minValue));
    EXPECT_TRUE(isNaN(result["h"].maxValue));
    EXPECT_TRUE(isNaN(result["h"].noValue));
    EXPECT_EQ(result["h"].format, "");
    EXPECT_EQ(result["h"].minColor, "");
    EXPECT_EQ(result["h"].maxColor, "");
}

// ============================================================================
// Test 3: Simple format with multiple fields
// ============================================================================
TEST(SettingParameterParseSubstates, SimpleFormatMultipleFields)
{
    SettingParameter sp;
    sp.substates = "h,z,tmp";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 3);
    EXPECT_TRUE(result.count("h"));
    EXPECT_TRUE(result.count("z"));
    EXPECT_TRUE(result.count("tmp"));
}

// ============================================================================
// Test 4: Extended format with name and format only
// ============================================================================
TEST(SettingParameterParseSubstates, ExtendedFormatNameAndFormat)
{
    SettingParameter sp;
    sp.substates = "(h,%f)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("h"));
    EXPECT_EQ(result["h"].name, "h");
    EXPECT_EQ(result["h"].format, "%f");
    EXPECT_TRUE(isNaN(result["h"].minValue));
    EXPECT_TRUE(isNaN(result["h"].maxValue));
    EXPECT_TRUE(isNaN(result["h"].noValue));
}

// ============================================================================
// Test 5: Extended format with name, format, min, max
// ============================================================================
TEST(SettingParameterParseSubstates, ExtendedFormatWithMinMax)
{
    SettingParameter sp;
    sp.substates = "(h,%f,1,100)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("h"));
    EXPECT_EQ(result["h"].name, "h");
    EXPECT_EQ(result["h"].format, "%f");
    EXPECT_EQ(result["h"].minValue, 1.0);
    EXPECT_EQ(result["h"].maxValue, 100.0);
    EXPECT_TRUE(isNaN(result["h"].noValue));
    EXPECT_EQ(result["h"].minColor, "");
    EXPECT_EQ(result["h"].maxColor, "");
}

// ============================================================================
// Test 6: Extended format with no-value only
// ============================================================================
TEST(SettingParameterParseSubstates, ExtendedFormatWithNoValueOnly)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,-1)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("tmp"));
    EXPECT_EQ(result["tmp"].name, "tmp");
    EXPECT_EQ(result["tmp"].format, "%f");
    EXPECT_TRUE(isNaN(result["tmp"].minValue));
    EXPECT_TRUE(isNaN(result["tmp"].maxValue));
    EXPECT_EQ(result["tmp"].noValue, -1.0);
}

// ============================================================================
// Test 7: Extended format with min, max, and no-value
// ============================================================================
TEST(SettingParameterParseSubstates, ExtendedFormatWithMinMaxNoValue)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,1,100,-1)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("tmp"));
    EXPECT_EQ(result["tmp"].minValue, 1.0);
    EXPECT_EQ(result["tmp"].maxValue, 100.0);
    EXPECT_EQ(result["tmp"].noValue, -1.0);
}

// ============================================================================
// Test 8: Extended format with colors
// ============================================================================
TEST(SettingParameterParseSubstates, ExtendedFormatWithColors)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,1,100,-1,#000011,#0011ff)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("tmp"));
    EXPECT_EQ(result["tmp"].minValue, 1.0);
    EXPECT_EQ(result["tmp"].maxValue, 100.0);
    EXPECT_EQ(result["tmp"].noValue, -1.0);
    EXPECT_EQ(result["tmp"].minColor, "#000011");
    EXPECT_EQ(result["tmp"].maxColor, "#0011ff");
}

// ============================================================================
// Test 9: Multiple extended formats
// ============================================================================
TEST(SettingParameterParseSubstates, MultipleExtendedFormats)
{
    SettingParameter sp;
    sp.substates = "(h,%f,1,100),(z,%f,400,1400)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result.count("h"));
    EXPECT_TRUE(result.count("z"));
    EXPECT_EQ(result["h"].minValue, 1.0);
    EXPECT_EQ(result["h"].maxValue, 100.0);
    EXPECT_EQ(result["z"].minValue, 400.0);
    EXPECT_EQ(result["z"].maxValue, 1400.0);
}

// ============================================================================
// Test 10: Mixed simple and extended formats
// ============================================================================
TEST(SettingParameterParseSubstates, MixedFormats)
{
    SettingParameter sp;
    sp.substates = "h,(z,%f,1,100),tmp";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 3);
    EXPECT_TRUE(result.count("h"));
    EXPECT_TRUE(result.count("z"));
    EXPECT_TRUE(result.count("tmp"));
    EXPECT_TRUE(isNaN(result["h"].minValue));
    EXPECT_EQ(result["z"].minValue, 1.0);
    EXPECT_TRUE(isNaN(result["tmp"].minValue));
}

// ============================================================================
// Test 11: Format with whitespace
// ============================================================================
TEST(SettingParameterParseSubstates, FormatWithWhitespace)
{
    SettingParameter sp;
    sp.substates = "  h  ,  ( z , %f , 1 , 100 )  , tmp  ";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 3);
    EXPECT_TRUE(result.count("h"));
    EXPECT_TRUE(result.count("z"));
    EXPECT_TRUE(result.count("tmp"));
    EXPECT_EQ(result["z"].minValue, 1.0);
    EXPECT_EQ(result["z"].maxValue, 100.0);
}

// ============================================================================
// Test 12: Invalid hex colors (should be ignored)
// ============================================================================
TEST(SettingParameterParseSubstates, InvalidHexColors)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,1,100,-1,invalid,#0011ff)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("tmp"));
    // Invalid color should be ignored
    EXPECT_EQ(result["tmp"].minColor, "");
    // Valid color should be accepted
    EXPECT_EQ(result["tmp"].maxColor, "#0011ff");
}

// ============================================================================
// Test 13: Invalid numeric values (should be NaN)
// ============================================================================
TEST(SettingParameterParseSubstates, InvalidNumericValues)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,invalid,100,-1)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("tmp"));
    // Invalid min should be NaN
    EXPECT_TRUE(isNaN(result["tmp"].minValue));
    // Valid max should be parsed
    EXPECT_EQ(result["tmp"].maxValue, 100.0);
}

// ============================================================================
// Test 14: Real-world example from config file
// ============================================================================
TEST(SettingParameterParseSubstates, RealWorldExample)
{
    SettingParameter sp;
    sp.substates = "(h,%f,1,100),(z,%f,400,1400),(tmp,%f,0,10,-1)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 3);
    
    EXPECT_EQ(result["h"].minValue, 1.0);
    EXPECT_EQ(result["h"].maxValue, 100.0);
    
    EXPECT_EQ(result["z"].minValue, 400.0);
    EXPECT_EQ(result["z"].maxValue, 1400.0);
    
    EXPECT_EQ(result["tmp"].minValue, 0.0);
    EXPECT_EQ(result["tmp"].maxValue, 10.0);
    EXPECT_EQ(result["tmp"].noValue, -1.0);
}

// ============================================================================
// Test 15: Negative values
// ============================================================================
TEST(SettingParameterParseSubstates, NegativeValues)
{
    SettingParameter sp;
    sp.substates = "(temp,%f,-50,50,-999)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("temp"));
    EXPECT_EQ(result["temp"].minValue, -50.0);
    EXPECT_EQ(result["temp"].maxValue, 50.0);
    EXPECT_EQ(result["temp"].noValue, -999.0);
}

// ============================================================================
// Test 16: Floating point values
// ============================================================================
TEST(SettingParameterParseSubstates, FloatingPointValues)
{
    SettingParameter sp;
    sp.substates = "(value,%f,0.5,99.9,-0.1)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_TRUE(result.count("value"));
    EXPECT_DOUBLE_EQ(result["value"].minValue, 0.5);
    EXPECT_DOUBLE_EQ(result["value"].maxValue, 99.9);
    EXPECT_DOUBLE_EQ(result["value"].noValue, -0.1);
}

// ============================================================================
// Test Suite: SettingParameter::getSubstateFields()
// ============================================================================

TEST(SettingParameterGetSubstateFields, EmptyString)
{
    SettingParameter sp;
    sp.substates = "";
    
    auto fields = sp.getSubstateFields();
    EXPECT_TRUE(fields.empty());
}

TEST(SettingParameterGetSubstateFields, SimpleFormat)
{
    SettingParameter sp;
    sp.substates = "h,z,tmp";
    
    auto fields = sp.getSubstateFields();
    EXPECT_EQ(fields.size(), 3);
    // Note: fields are returned from map which sorts alphabetically
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "h") != fields.end());
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "z") != fields.end());
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "tmp") != fields.end());
}

TEST(SettingParameterGetSubstateFields, ExtendedFormat)
{
    SettingParameter sp;
    sp.substates = "(h,%f,1,100),(z,%f,400,1400)";
    
    auto fields = sp.getSubstateFields();
    EXPECT_EQ(fields.size(), 2);
    EXPECT_EQ(fields[0], "h");
    EXPECT_EQ(fields[1], "z");
}

TEST(SettingParameterGetSubstateFields, MixedFormat)
{
    SettingParameter sp;
    sp.substates = "h,(z,%f,1,100),tmp";
    
    auto fields = sp.getSubstateFields();
    EXPECT_EQ(fields.size(), 3);
    // Note: fields are returned from map which sorts alphabetically
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "h") != fields.end());
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "z") != fields.end());
    EXPECT_TRUE(std::find(fields.begin(), fields.end(), "tmp") != fields.end());
}

// ============================================================================
// Test Suite: SettingParameter::initializeSubstateInfo()
// ============================================================================

TEST(SettingParameterInitializeSubstateInfo, EmptyString)
{
    SettingParameter sp;
    sp.substates = "";
    sp.initializeSubstateInfo();
    
    EXPECT_TRUE(sp.substateInfo.empty());
}

TEST(SettingParameterInitializeSubstateInfo, SimpleFormat)
{
    SettingParameter sp;
    sp.substates = "h,z";
    sp.initializeSubstateInfo();
    
    EXPECT_EQ(sp.substateInfo.size(), 2);
    EXPECT_TRUE(sp.substateInfo.count("h"));
    EXPECT_TRUE(sp.substateInfo.count("z"));
    EXPECT_EQ(sp.substateInfo["h"].name, "h");
    EXPECT_EQ(sp.substateInfo["z"].name, "z");
}

TEST(SettingParameterInitializeSubstateInfo, ExtendedFormat)
{
    SettingParameter sp;
    sp.substates = "(h,%f,1,100),(z,%f,400,1400)";
    sp.initializeSubstateInfo();
    
    EXPECT_EQ(sp.substateInfo.size(), 2);
    EXPECT_EQ(sp.substateInfo["h"].minValue, 1.0);
    EXPECT_EQ(sp.substateInfo["h"].maxValue, 100.0);
    EXPECT_EQ(sp.substateInfo["z"].minValue, 400.0);
    EXPECT_EQ(sp.substateInfo["z"].maxValue, 1400.0);
}

TEST(SettingParameterInitializeSubstateInfo, WithColors)
{
    SettingParameter sp;
    sp.substates = "(tmp,%f,1,100,-1,#000011,#0011ff)";
    sp.initializeSubstateInfo();
    
    EXPECT_EQ(sp.substateInfo.size(), 1);
    EXPECT_EQ(sp.substateInfo["tmp"].minColor, "#000011");
    EXPECT_EQ(sp.substateInfo["tmp"].maxColor, "#0011ff");
}

// ============================================================================
// Test Suite: Edge cases and complex scenarios
// ============================================================================

TEST(SettingParameterParseSubstates, ComplexRealWorldScenario)
{
    SettingParameter sp;
    // Complex scenario with mixed formats and all features
    sp.substates = "simple,(extended,%f,0,100),(withNoValue,%f,10,90,-999),(withColors,%f,5,95,-1,#ff0000,#00ff00),another";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 5);
    
    // Simple field
    EXPECT_EQ(result["simple"].name, "simple");
    EXPECT_TRUE(isNaN(result["simple"].minValue));
    
    // Extended field
    EXPECT_EQ(result["extended"].minValue, 0.0);
    EXPECT_EQ(result["extended"].maxValue, 100.0);
    
    // With no-value
    EXPECT_EQ(result["withNoValue"].noValue, -999.0);
    
    // With colors
    EXPECT_EQ(result["withColors"].minColor, "#ff0000");
    EXPECT_EQ(result["withColors"].maxColor, "#00ff00");
    
    // Another simple
    EXPECT_EQ(result["another"].name, "another");
}

TEST(SettingParameterParseSubstates, LargeNumberValues)
{
    SettingParameter sp;
    sp.substates = "(pressure,%f,1000000,9999999)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result["pressure"].minValue, 1000000.0);
    EXPECT_EQ(result["pressure"].maxValue, 9999999.0);
}

TEST(SettingParameterParseSubstates, ScientificNotation)
{
    SettingParameter sp;
    sp.substates = "(energy,%f,1e-5,1e5)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_DOUBLE_EQ(result["energy"].minValue, 1e-5);
    EXPECT_DOUBLE_EQ(result["energy"].maxValue, 1e5);
}

TEST(SettingParameterParseSubstates, AllHexColorVariations)
{
    SettingParameter sp;
    sp.substates = "(test,%f,0,100,0,#000000,#FFFFFF)";
    
    auto result = sp.parseSubstates();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result["test"].minColor, "#000000");
    EXPECT_EQ(result["test"].maxColor, "#FFFFFF");
}
