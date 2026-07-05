#pragma once
#include <string>
#include <regex>

struct CodeAndComment {
    std::string code;
    std::string comment;
};

CodeAndComment splitComment(const std::string& line);
std::string replaceOutsideStrings(const std::string& line, const std::regex& reg, const std::string& replacement);
