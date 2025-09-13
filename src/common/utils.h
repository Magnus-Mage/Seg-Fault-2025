#ifndef FORTH_UTILS_H
#define FORTH_UTILS_H

#include <string>
#include <algorithm>
#include <cctype>
#include <filesystem>  

class ForthUtils {
public:
    // String utilities
    static std::string trim(const std::string& str) {
        auto start = str.begin();
        while (start != str.end() && std::isspace(*start)) {
            start++;
        }
        
        auto end = str.end();
        do {
            end--;
        } while (std::distance(start, end) > 0 && std::isspace(*end));
        
        return std::string(start, end + 1);
    }
    
    static std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), 
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }
    
    // Number parsing
    static bool isNumber(const std::string& str) {
        if (str.empty()) return false;
        
        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') start = 1;
        if (start >= str.length()) return false;
        
        bool hasDecimal = false;
        for (size_t i = start; i < str.length(); ++i) {
            if (str[i] == '.') {
                if (hasDecimal) return false; // Multiple decimal points
                hasDecimal = true;
            } else if (!std::isdigit(str[i])) {
                return false;
            }
        }
        return true;
    }
    
    // FORTH word validation
    static bool isValidWordName(const std::string& str) {
        if (str.empty()) return false;
        
        // FORTH words can contain most printable characters except spaces
        for (char c : str) {
            if (std::isspace(c) || c == ':' || c == ';') {
                return false;
            }
        }
        return true;
    }
    
    // Path utilities 
    static bool createDirectories(const std::string& path) {
        return std::filesystem::create_directories(path);
    }
    
    static bool fileExists(const std::string& path) {
        return std::filesystem::exists(path);
    }
};

#endif // FORTH_UTILS_H
