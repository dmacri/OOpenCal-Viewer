#include <iostream>
#include <cctype>
#include <ranges>
#include "SettingParameter.h"


// Helper function to trim whitespace
static std::string trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Helper function to validate hex color format
static bool isValidHexColor(const std::string& color)
{
    if (color.empty())
        return false;
    if (color[0] != '#' || color.length() != 7)
        return false;
    for (size_t i = 1; i < color.length(); ++i)
    {
        if (! std::isxdigit(color[i]))
            return false;
    }
    return true;
}

std::map<std::string, SubstateInfo> SettingParameter::parseSubstates() const
{
    std::map<std::string, SubstateInfo> result;
    
    if (substates.empty())
        return result;
    
    // Parse substates string
    // Format can be:
    // - Simple: "h,z" or "s"
    // - Extended: "(h,%f,1,100)" or "(h,%f,-1)" or "(h,%f,1,100,-1)" or "(h,%f,1,100,-1,#000011,#0011ff)"
    // - Mixed: "h,(z,%f,1,100),s"
    
    std::string current = substates;
    size_t pos = 0;
    
    while (pos < current.length())
    {
        // Skip whitespace and commas
        while (pos < current.length() && (std::isspace(current[pos]) || current[pos] == ','))
            pos++;
        
        if (pos >= current.length())
            break;
        
        SubstateInfo info;
        
        // Check if this is extended format (starts with '(')
        if (current[pos] == '(')
        {
            // Extended format: (fieldName,format,minValue,maxValue,noValue,minColor,maxColor)
            size_t start = pos + 1;
            size_t end = current.find(')', pos);
            if (end == std::string::npos)
                break;
            
            std::string content = current.substr(start, end - start);
            
            // Split by comma
            std::vector<std::string> parts;
            size_t partPos = 0;
            while (partPos < content.length())
            {
                size_t partEnd = content.find(',', partPos);
                if (partEnd == std::string::npos)
                    partEnd = content.length();
                
                parts.push_back(trim(content.substr(partPos, partEnd - partPos)));
                partPos = partEnd + 1;
            }
            
            // Extract parameters
            if (parts.size() >= 1)
                info.name = parts[0];
            if (parts.size() >= 2)
                info.format = parts[1];
            
            // Handle different parameter counts:
            // 3 params: (name, format, noValue)
            // 4 params: (name, format, minValue, maxValue)
            // 5+ params: (name, format, minValue, maxValue, noValue, minColor, maxColor)
            if (parts.size() == 3)
            {
                // (name, format, noValue)
                try
                {
                    info.noValue = std::stod(parts[2]);
                }
                catch (const std::exception&)
                {
                    info.noValue = std::numeric_limits<double>::quiet_NaN();
                }
            }
            else if (parts.size() >= 4)
            {
                // (name, format, minValue, maxValue, ...)
                try
                {
                    info.minValue = std::stod(parts[2]);
                }
                catch (const std::exception&)
                {
                    info.minValue = std::numeric_limits<double>::quiet_NaN();
                }
                try
                {
                    info.maxValue = std::stod(parts[3]);
                }
                catch (const std::exception&)
                {
                    info.maxValue = std::numeric_limits<double>::quiet_NaN();
                }
                
                if (parts.size() >= 5)
                {
                    try
                    {
                        info.noValue = std::stod(parts[4]);
                    }
                    catch (const std::exception&)
                    {
                        info.noValue = std::numeric_limits<double>::quiet_NaN();
                    }
                }
                if (parts.size() >= 6)
                {
                    if (isValidHexColor(parts[5]))
                        info.minColor = parts[5];
                }
                if (parts.size() >= 7)
                {
                    if (isValidHexColor(parts[6]))
                        info.maxColor = parts[6];
                }
            }
            
            pos = end + 1;
        }
        else
        {
            // Simple format: extract until comma or '(' (next extended group)
            size_t end = current.find(',', pos);
            size_t paren = current.find('(', pos);
            
            // Find which comes first
            if (paren != std::string::npos && (end == std::string::npos || paren < end))
                end = paren;
            
            if (end == std::string::npos)
                end = current.length();
            
            info.name = trim(current.substr(pos, end - pos));
            pos = end;
        }
        
        if (!info.name.empty())
            result[info.name] = info;
    }
    
    return result;
}

std::vector<std::string> SettingParameter::getSubstateFields() const
{
    auto substateFieldsRange = parseSubstates() | std::views::transform([](const auto& p) { return p.first; });
    return {substateFieldsRange.begin(), substateFieldsRange.end()};
}

void SettingParameter::initializeSubstateInfo()
{
    // Use parseSubstates() to populate substateInfo
    substateInfo = parseSubstates();
}

std::ostream& operator<<(std::ostream& os, const SettingParameter& sp)
{
    os << "SettingParameter{"
       << "numberOfColumnX=" << sp.numberOfColumnX << ", "
       << "numberOfRowsY=" << sp.numberOfRowsY << ", "
       << "nNodeX=" << sp.nNodeX << ", "
       << "nNodeY=" << sp.nNodeY << ", "
       << "outputFileName=" << sp.outputFileName << "}";
    return os;
}
