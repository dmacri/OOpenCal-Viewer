# Substate Parsing Extension

## Overview

The `SettingParameter` class has been extended to support enhanced substate field definitions with color gradients and no-value indicators. This document describes the new parsing capabilities and format specifications.

## Supported Formats

### 1. Simple Format
Single field name without parameters:
```
h
z
tmp
```

Multiple simple fields (comma-separated):
```
h,z,tmp
```

### 2. Extended Format with Name and Format Only
```
(h,%f)
(z,%d)
```

### 3. Extended Format with Min/Max Values
```
(h,%f,1,100)
(z,%f,400,1400)
```

### 4. Extended Format with No-Value Only
When only 3 parameters are provided, the third is treated as `noValue`:
```
(tmp,%f,-1)
```

This format is useful when you want to mark certain values as "no data" without specifying min/max bounds.

### 5. Extended Format with Min, Max, and No-Value
```
(tmp,%f,1,100,-1)
```

The no-value parameter indicates which value should be treated as "no data" (e.g., -1 or -999).

### 6. Extended Format with Colors
Complete format with all parameters including color gradients:
```
(tmp,%f,1,100,-1,#000011,#0011ff)
```

Parameters:
- `tmp` - field name
- `%f` - printf format string
- `1` - minimum value
- `100` - maximum value
- `-1` - no-value indicator
- `#000011` - hex color for minimum value
- `#0011ff` - hex color for maximum value

### 7. Mixed Formats
You can mix simple and extended formats:
```
h,(z,%f,1,100),tmp
simple,(extended,%f,0,100),(withNoValue,%f,10,90,-999),(withColors,%f,5,95,-1,#ff0000,#00ff00),another
```

## SubstateInfo Structure

```cpp
struct SubstateInfo
{
    std::string name;                                           // Field name (e.g., "h", "z")
    double minValue = std::numeric_limits<double>::quiet_NaN(); // Minimum value for display
    double maxValue = std::numeric_limits<double>::quiet_NaN(); // Maximum value for display
    std::string format = "";                                    // Printf format string
    double noValue = std::numeric_limits<double>::quiet_NaN();  // Value representing "no data"
    std::string minColor = "";                                  // Hex color for minimum (e.g., "#000011")
    std::string maxColor = "";                                  // Hex color for maximum (e.g., "#0011ff")
};
```

## API

### parseSubstates()
```cpp
std::map<std::string, SubstateInfo> parseSubstates() const;
```

Main parsing function that handles all format variations. Returns a map of field name to SubstateInfo.

**Example:**
```cpp
SettingParameter sp;
sp.substates = "(h,%f,1,100,-1,#000011,#0011ff),(z,%f,400,1400)";
auto result = sp.parseSubstates();

// result["h"].minValue = 1.0
// result["h"].maxValue = 100.0
// result["h"].noValue = -1.0
// result["h"].minColor = "#000011"
// result["h"].maxColor = "#0011ff"

// result["z"].minValue = 400.0
// result["z"].maxValue = 1400.0
```

### getSubstateFields()
```cpp
std::vector<std::string> getSubstateFields() const;
```

Returns a vector of field names extracted from the substates string. Uses `parseSubstates()` internally.

**Example:**
```cpp
SettingParameter sp;
sp.substates = "h,(z,%f,1,100),tmp";
auto fields = sp.getSubstateFields();
// fields contains: "h", "z", "tmp" (order may vary due to map sorting)
```

### initializeSubstateInfo()
```cpp
void initializeSubstateInfo();
```

Populates the `substateInfo` map from the `substates` string. Should be called after the `substates` field is set.

**Example:**
```cpp
SettingParameter sp;
sp.substates = "(h,%f,1,100),(z,%f,400,1400)";
sp.initializeSubstateInfo();

// sp.substateInfo["h"].minValue = 1.0
// sp.substateInfo["z"].minValue = 400.0
```

## Configuration File Format

In configuration files (e.g., `Header.txt`), the VISUALIZATION section can now include:

```
VISUALIZATION:
    mode=text
    substates=h,(z,%f,1,100),(tmp,%f,0,10,-1)
    reduction=sum,min,max
```

Or with colors:

```
VISUALIZATION:
    mode=binary
    substates=(h,%f,0,100,-1,#000000,#ff0000),(z,%f,400,1400,-1,#0000ff,#00ffff)
    reduction=sum,min,max
```

## Parsing Rules

1. **Whitespace Handling**: Leading and trailing whitespace is automatically trimmed from all parameters.

2. **Comma Separation**: Fields are separated by commas at the top level. Commas inside parentheses are treated as parameter separators.

3. **Parameter Count Logic**:
   - 2 params: `(name, format)` - only name and format
   - 3 params: `(name, format, noValue)` - name, format, and no-value
   - 4+ params: `(name, format, minValue, maxValue, ...)` - full extended format

4. **Numeric Parsing**: Invalid numeric values are converted to NaN. This allows graceful handling of malformed input.

5. **Color Validation**: Hex colors must be in format `#RRGGBB` (7 characters total). Invalid colors are silently ignored.

6. **Error Handling**: The parser is fault-tolerant:
   - Invalid numbers → NaN
   - Invalid colors → empty string
   - Malformed parentheses → skipped
   - Empty field names → ignored

## Examples

### Example 1: Simple Configuration
```cpp
SettingParameter sp;
sp.substates = "h,z,tmp";
sp.initializeSubstateInfo();

// Result:
// sp.substateInfo["h"] = {name: "h", minValue: NaN, maxValue: NaN, format: "", noValue: NaN}
// sp.substateInfo["z"] = {name: "z", minValue: NaN, maxValue: NaN, format: "", noValue: NaN}
// sp.substateInfo["tmp"] = {name: "tmp", minValue: NaN, maxValue: NaN, format: "", noValue: NaN}
```

### Example 2: Extended with Min/Max
```cpp
SettingParameter sp;
sp.substates = "(h,%f,1,100),(z,%f,400,1400)";
sp.initializeSubstateInfo();

// Result:
// sp.substateInfo["h"] = {name: "h", minValue: 1.0, maxValue: 100.0, format: "%f", noValue: NaN}
// sp.substateInfo["z"] = {name: "z", minValue: 400.0, maxValue: 1400.0, format: "%f", noValue: NaN}
```

### Example 3: With No-Value
```cpp
SettingParameter sp;
sp.substates = "(tmp,%f,-1)";
sp.initializeSubstateInfo();

// Result:
// sp.substateInfo["tmp"] = {name: "tmp", minValue: NaN, maxValue: NaN, format: "%f", noValue: -1.0}
```

### Example 4: Complete with Colors
```cpp
SettingParameter sp;
sp.substates = "(pressure,%f,1000,9999,-1,#0000ff,#ff0000)";
sp.initializeSubstateInfo();

// Result:
// sp.substateInfo["pressure"] = {
//     name: "pressure",
//     minValue: 1000.0,
//     maxValue: 9999.0,
//     format: "%f",
//     noValue: -1.0,
//     minColor: "#0000ff",
//     maxColor: "#ff0000"
// }
```

### Example 5: Mixed Formats
```cpp
SettingParameter sp;
sp.substates = "simple,(extended,%f,0,100),(withColors,%f,5,95,-1,#ff0000,#00ff00)";
sp.initializeSubstateInfo();

// Result:
// sp.substateInfo["simple"] = {name: "simple", minValue: NaN, maxValue: NaN, ...}
// sp.substateInfo["extended"] = {name: "extended", minValue: 0.0, maxValue: 100.0, ...}
// sp.substateInfo["withColors"] = {name: "withColors", minValue: 5.0, maxValue: 95.0, 
//                                   noValue: -1.0, minColor: "#ff0000", maxColor: "#00ff00"}
```

## Testing

Comprehensive unit tests are available in `tests/SettingParameterTests.cpp`. The test suite includes:

- **20 tests** for `parseSubstates()` covering:
  - Empty strings
  - Simple formats (single and multiple fields)
  - Extended formats (all parameter combinations)
  - Mixed formats
  - Whitespace handling
  - Invalid input handling
  - Real-world scenarios
  - Edge cases (negative values, scientific notation, large numbers)

- **4 tests** for `getSubstateFields()`
- **4 tests** for `initializeSubstateInfo()`

**Run tests:**
```bash
cd build
cmake --build . -j4
./tests/SettingParameterTests --gtest_color=yes
```

**Expected output:**
```
[==========] 28 tests from 3 test suites ran.
[  PASSED  ] 28 tests.
```

## Implementation Details

### Refactoring

The implementation was refactored to eliminate code duplication:

1. **New `parseSubstates()` function**: Central parsing logic that handles all format variations.

2. **Simplified `getSubstateFields()`**: Now uses `parseSubstates()` internally and extracts only field names.

3. **Simplified `initializeSubstateInfo()`**: Now uses `parseSubstates()` internally to populate the map.

This approach ensures:
- Single source of truth for parsing logic
- Easier maintenance and bug fixes
- Consistent behavior across all parsing functions

### Helper Functions

- `trim(const std::string& str)`: Removes leading/trailing whitespace
- `isValidHexColor(const std::string& color)`: Validates hex color format (#RRGGBB)

## Backward Compatibility

All existing code using the old format continues to work:
- Simple formats: `"h,z"` ✓
- Extended formats: `"(h,%f,1,100)"` ✓
- Mixed formats: `"h,(z,%f,1,100)"` ✓

New features are opt-in:
- No-value parameter: `"(tmp,%f,-1)"` (new)
- Color gradients: `"(tmp,%f,1,100,-1,#000011,#0011ff)"` (new)

## Future Enhancements

Possible future improvements:
1. Support for alpha channel in colors: `#RRGGBBAA`
2. Named color support: `"red"`, `"blue"`, etc.
3. Color interpolation strategies: `"linear"`, `"logarithmic"`, etc.
4. Range validation: ensure minValue < maxValue
5. Automatic range detection from data files
