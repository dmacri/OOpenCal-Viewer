# Substate Parsing Tests

## Overview

This document describes the comprehensive test suite for the extended substate parsing functionality in `SettingParameter`.

## Test File Location

`tests/SettingParameterTests.cpp`

## Test Statistics

- **Total Tests**: 28
- **Test Suites**: 3
- **Pass Rate**: 100% ✓

### Test Breakdown

| Test Suite | Count | Coverage |
|-----------|-------|----------|
| `SettingParameterParseSubstates` | 20 | Main parsing logic with all format variations |
| `SettingParameterGetSubstateFields` | 4 | Field name extraction |
| `SettingParameterInitializeSubstateInfo` | 4 | Substate info initialization |

## Test Suites

### 1. SettingParameterParseSubstates (20 tests)

Tests for the main `parseSubstates()` function covering all supported formats and edge cases.

#### Basic Format Tests

- **EmptyString**: Handles empty substates string
- **SimpleFormatSingleField**: Parses single field name (e.g., "h")
- **SimpleFormatMultipleFields**: Parses multiple comma-separated fields (e.g., "h,z,tmp")

#### Extended Format Tests

- **ExtendedFormatNameAndFormat**: Parses name and format only (e.g., "(h,%f)")
- **ExtendedFormatWithMinMax**: Parses name, format, min, max (e.g., "(h,%f,1,100)")
- **ExtendedFormatWithNoValueOnly**: Parses name, format, no-value (e.g., "(tmp,%f,-1)")
- **ExtendedFormatWithMinMaxNoValue**: Parses name, format, min, max, no-value (e.g., "(tmp,%f,1,100,-1)")
- **ExtendedFormatWithColors**: Parses complete format with colors (e.g., "(tmp,%f,1,100,-1,#000011,#0011ff)")

#### Multiple and Mixed Format Tests

- **MultipleExtendedFormats**: Parses multiple extended formats (e.g., "(h,%f,1,100),(z,%f,400,1400)")
- **MixedFormats**: Parses mixed simple and extended (e.g., "h,(z,%f,1,100),tmp")

#### Whitespace and Validation Tests

- **FormatWithWhitespace**: Handles extra whitespace in all positions
- **InvalidHexColors**: Validates hex color format, ignores invalid colors
- **InvalidNumericValues**: Converts invalid numbers to NaN

#### Real-World and Edge Case Tests

- **RealWorldExample**: Tests actual config file format (e.g., "(h,%f,1,100),(z,%f,400,1400),(tmp,%f,0,10,-1)")
- **NegativeValues**: Handles negative min, max, and no-value
- **FloatingPointValues**: Parses decimal values (e.g., 0.5, 99.9, -0.1)
- **ComplexRealWorldScenario**: Tests complex mixed format with all features
- **LargeNumberValues**: Handles large numbers (e.g., 1000000, 9999999)
- **ScientificNotation**: Parses scientific notation (e.g., 1e-5, 1e5)
- **AllHexColorVariations**: Tests various valid hex colors (#000000, #FFFFFF, etc.)

### 2. SettingParameterGetSubstateFields (4 tests)

Tests for the `getSubstateFields()` function that extracts field names.

- **EmptyString**: Returns empty vector for empty substates
- **SimpleFormat**: Extracts names from simple format
- **ExtendedFormat**: Extracts names from extended format
- **MixedFormat**: Extracts names from mixed format

**Note**: Tests use `std::find()` to verify field presence because the underlying map sorts keys alphabetically.

### 3. SettingParameterInitializeSubstateInfo (4 tests)

Tests for the `initializeSubstateInfo()` function that populates the substateInfo map.

- **EmptyString**: Produces empty map for empty substates
- **SimpleFormat**: Creates SubstateInfo entries for simple format
- **ExtendedFormat**: Populates all fields for extended format
- **WithColors**: Correctly stores color values

## Running the Tests

### Build Tests

```bash
cd /home/agh/Pulpit/ItaliaSoftware/Qt-VTK-viewer
cmake --build build -j4
```

### Run Tests

```bash
./build/tests/SettingParameterTests --gtest_color=yes
```

### Run Specific Test Suite

```bash
./build/tests/SettingParameterTests --gtest_filter=SettingParameterParseSubstates.*
```

### Run Specific Test

```bash
./build/tests/SettingParameterTests --gtest_filter=SettingParameterParseSubstates.ExtendedFormatWithColors
```

### Verbose Output

```bash
./build/tests/SettingParameterTests --gtest_color=yes -v
```

## Test Coverage

### Format Variations Covered

- ✓ Simple format: `h`, `h,z,tmp`
- ✓ Extended format (2 params): `(h,%f)`
- ✓ Extended format (3 params): `(tmp,%f,-1)`
- ✓ Extended format (4 params): `(h,%f,1,100)`
- ✓ Extended format (5 params): `(tmp,%f,1,100,-1)`
- ✓ Extended format (7 params): `(tmp,%f,1,100,-1,#000011,#0011ff)`
- ✓ Mixed formats: `h,(z,%f,1,100),tmp`

### Value Types Covered

- ✓ Integers: `1`, `100`, `400`
- ✓ Negative numbers: `-1`, `-50`, `-999`
- ✓ Floating point: `0.5`, `99.9`, `-0.1`
- ✓ Large numbers: `1000000`, `9999999`
- ✓ Scientific notation: `1e-5`, `1e5`
- ✓ Invalid numbers: `invalid`, `abc` (converted to NaN)

### Color Formats Covered

- ✓ Valid hex colors: `#000000`, `#FFFFFF`, `#000011`, `#0011ff`, `#ff0000`, `#00ff00`
- ✓ Invalid hex colors: `invalid`, `#00001` (too short), `#0000001` (too long), `#GGGGGG` (invalid hex)

### Edge Cases Covered

- ✓ Empty strings
- ✓ Whitespace handling (leading, trailing, internal)
- ✓ Malformed input (invalid numbers, invalid colors)
- ✓ Boundary conditions (single field, many fields)
- ✓ Special characters in format strings (`%f`, `%d`, `%e`)

## Test Output Example

```
[==========] Running 28 tests from 3 test suites.
[----------] Global test environment set-up.
[----------] 20 tests from SettingParameterParseSubstates
[ RUN      ] SettingParameterParseSubstates.EmptyString
[       OK ] SettingParameterParseSubstates.EmptyString (0 ms)
...
[----------] 20 tests from SettingParameterParseSubstates (0 ms total)

[----------] 4 tests from SettingParameterGetSubstateFields
[ RUN      ] SettingParameterGetSubstateFields.EmptyString
[       OK ] SettingParameterGetSubstateFields.EmptyString (0 ms)
...
[----------] 4 tests from SettingParameterGetSubstateFields (0 ms total)

[----------] 4 tests from SettingParameterInitializeSubstateInfo
[ RUN      ] SettingParameterInitializeSubstateInfo.EmptyString
[       OK ] SettingParameterInitializeSubstateInfo.EmptyString (0 ms)
...
[----------] 4 tests from SettingParameterInitializeSubstateInfo (0 ms total)

[----------] Global test environment tear-down
[==========] 28 tests from 3 test suites ran. (0 ms total)
[  PASSED  ] 28 tests.
```

## Key Test Assertions

### NaN Checking

Tests use `std::isnan()` to verify NaN values:
```cpp
EXPECT_TRUE(isNaN(result["h"].minValue));
EXPECT_FALSE(isNaN(result["h"].minValue));
```

### Value Comparison

Tests use exact equality for numeric values:
```cpp
EXPECT_EQ(result["h"].minValue, 1.0);
EXPECT_DOUBLE_EQ(result["energy"].minValue, 1e-5);
```

### String Comparison

Tests verify string values:
```cpp
EXPECT_EQ(result["h"].format, "%f");
EXPECT_EQ(result["tmp"].minColor, "#000011");
```

### Container Verification

Tests verify map contents:
```cpp
EXPECT_EQ(result.size(), 3);
EXPECT_TRUE(result.count("h"));
EXPECT_TRUE(std::find(fields.begin(), fields.end(), "h") != fields.end());
```

## Integration with CI/CD

The test suite can be integrated into continuous integration pipelines:

```bash
# Build and run all tests
cmake --build build -j4 && ./build/tests/SettingParameterTests

# With exit code checking
if ./build/tests/SettingParameterTests; then
    echo "All tests passed"
else
    echo "Tests failed"
    exit 1
fi
```

## Maintenance

### Adding New Tests

When adding new format variations or edge cases:

1. Add test to appropriate suite in `tests/SettingParameterTests.cpp`
2. Follow naming convention: `TEST(SuiteName, DescriptiveName)`
3. Use existing helper functions (`isNaN()`)
4. Document the test purpose in comments
5. Run full test suite to verify

### Updating Tests

If parsing logic changes:

1. Review affected tests
2. Update test expectations if behavior changes
3. Add new tests for new behavior
4. Ensure backward compatibility tests still pass

## Known Limitations

- Map ordering: Fields are returned in alphabetical order due to `std::map` sorting
- Hex color validation: Only validates format, not actual color values
- Numeric precision: Uses standard C++ `double` precision
- No range validation: Doesn't check if minValue < maxValue

## Future Test Enhancements

Potential improvements:
1. Performance benchmarks for large substate strings
2. Stress tests with thousands of fields
3. Unicode handling in field names
4. Locale-specific number parsing
5. Memory leak detection with valgrind
