#include <iostream>
#include <cctype>
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

std::vector<std::string> SettingParameter::getSubstateFields() const
{
    std::vector<std::string> fields;
    
    if (substates.empty())
        return fields;
    
    // Parse substates string
    // Format can be:
    // - Simple: "h,z" or "s"
    // - Extended: "(h,f,0,1300),(z,f,0,5)"
    // - Mixed: "(h,f,1,100),z" or "z,(h,f,1,100)"
    
    std::string current = substates;
    size_t pos = 0;
    
    while (pos < current.length())
    {
        // Skip whitespace and commas
        while (pos < current.length() && (std::isspace(current[pos]) || current[pos] == ','))
            pos++;
        
        if (pos >= current.length())
            break;
        
        // Check if this is extended format (starts with '(')
        if (current[pos] == '(')
        {
            // Extended format: extract field name until first comma
            size_t start = pos + 1;
            size_t end = current.find(',', start);
            if (end != std::string::npos)
            {
                std::string fieldName = trim(current.substr(start, end - start));
                if (!fieldName.empty())
                    fields.push_back(fieldName);
            }
            
            // Skip to end of this group
            pos = current.find(')', pos);
            if (pos != std::string::npos)
                pos++;
            else
                break;
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
            
            std::string fieldName = trim(current.substr(pos, end - pos));
            if (!fieldName.empty())
                fields.push_back(fieldName);
            
            pos = end;
        }
    }
    
    return fields;
}

void SettingParameter::initializeSubstateInfo()
{
    // Clear existing info
    substateInfo.clear();
    
    if (substates.empty())
        return;
    
    // Parse substates string and extract parameters
    // Format can be:
    // - Simple: "h,z" or "s"
    // - Extended: "(h,f,0,1300),(z,f,0,5)"
    // - Mixed: "(h,f,1,100),z" or "z,(h,f,1,100)"
    
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
            // Extended format: (fieldName,format,minValue,maxValue)
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
            if (parts.size() >= 3)
            {
                try
                {
                    info.minValue = std::stod(parts[2]);
                }
                catch (const std::exception&)
                {
                    info.minValue = std::numeric_limits<double>::quiet_NaN();
                }
            }
            if (parts.size() >= 4)
            {
                try
                {
                    info.maxValue = std::stod(parts[3]);
                }
                catch (const std::exception&)
                {
                    info.maxValue = std::numeric_limits<double>::quiet_NaN();
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
            substateInfo[info.name] = info;
    }
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
