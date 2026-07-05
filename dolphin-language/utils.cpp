#include "utils.hpp"

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string getDirectoryPath(const std::string& filepath) {
    size_t found = filepath.find_last_of("/\\");
    if (found == std::string::npos) return "";
    return filepath.substr(0, found + 1);
}
