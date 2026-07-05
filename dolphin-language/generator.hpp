#pragma once
#include "parser.hpp"
#include <sstream>
#include <set>
#include <vector>

void transpileFile(const std::string& filepath,
                   std::stringstream& global_stream,
                   std::stringstream& main_stream,
                   std::vector<Scope>& scope_stack,
                   std::set<std::string>& imported_files);
