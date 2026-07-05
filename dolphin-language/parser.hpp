#pragma once
#include <string>
#include <set>
#include <vector>

enum ScopeType {
    SCOPE_BLOCK,
    SCOPE_INITIALIZER
};

struct Scope {
    ScopeType type;
    std::set<std::string> vars;
};

std::string translateArrayLiterals(const std::string& line, std::vector<bool>& replaced_brackets);
std::string translateLambdas(std::string line);
std::string translateObjects(std::string line);
