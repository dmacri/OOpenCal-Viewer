#include <iostream>
#include <cctype>
#include "SettingParameter.h"


std::vector<std::string> SettingParameter::getSubstateFields() const
{
    std::vector<std::string> fields;
    
    if (substates.empty())
        return fields;
    
    // Parse substates string
    // Format can be:
    // - Simple: "h,z" or "s"
    // - Extended: "(h,f,0,1300),(z,f,0,5)"
    
    std::string current = substates;
    size_t pos = 0;
    
    while (pos < current.length())
    {
        // Skip whitespace
        while (pos < current.length() && std::isspace(current[pos]))
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
                std::string fieldName = current.substr(start, end - start);
                // Trim whitespace
                fieldName.erase(0, fieldName.find_first_not_of(" \t"));
                fieldName.erase(fieldName.find_last_not_of(" \t") + 1);
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
            // Simple format: extract until comma or end
            size_t end = current.find(',', pos);
            if (end == std::string::npos)
                end = current.length();
            
            std::string fieldName = current.substr(pos, end - pos);
            // Trim whitespace
            fieldName.erase(0, fieldName.find_first_not_of(" \t"));
            fieldName.erase(fieldName.find_last_not_of(" \t") + 1);
            if (!fieldName.empty())
                fields.push_back(fieldName);
            
            pos = end;
        }
        
        // Skip comma
        if (pos < current.length() && current[pos] == ',')
            pos++;
    }
    
    return fields;
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
