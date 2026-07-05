#pragma once
#include <string>

std::string trim(const std::string& str);
std::string getDirectoryPath(const std::string& filepath);
std::string formatArgs(const std::string& args_str, const std::string& type_prefix = "var ");
