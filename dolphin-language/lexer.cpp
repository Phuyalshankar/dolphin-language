#include "lexer.hpp"

CodeAndComment splitComment(const std::string& line) {
    bool in_string = false;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            in_string = !in_string;
        } else if (!in_string && line[i] == '#') {
            return { line.substr(0, i), line.substr(i + 1) };
        }
    }
    return { line, "" };
}

std::string replaceOutsideStrings(const std::string& line, const std::regex& reg, const std::string& replacement) {
    std::string result = "";
    bool in_string = false;
    std::string current_chunk = "";
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            if (!in_string) {
                result += std::regex_replace(current_chunk, reg, replacement);
                current_chunk = "";
            }
            in_string = !in_string;
            result += line[i];
        } else if (in_string) {
            result += line[i];
        } else {
            current_chunk += line[i];
        }
    }
    if (!current_chunk.empty()) {
        result += std::regex_replace(current_chunk, reg, replacement);
    }
    return result;
}
