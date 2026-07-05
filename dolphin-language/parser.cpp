#include "parser.hpp"
#include "utils.hpp"
#include <vector>
#include <regex>

std::string translateArrayLiterals(const std::string& line, std::vector<bool>& replaced_brackets) {
    std::string result = "";
    bool in_string = false;
    for (size_t i = 0; i < line.length(); ++i) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            in_string = !in_string;
            result += line[i];
        } else if (in_string) {
            result += line[i];
        } else if (line[i] == '[') {
            bool is_indexing = false;
            if (i > 0) {
                char prev = line[i-1];
                if (isalnum(prev) || prev == '_' || prev == ']' || prev == ')') {
                    is_indexing = true;
                }
            }
            if (is_indexing) {
                result += '[';
                replaced_brackets.push_back(false);
            } else {
                result += "var_array{";
                replaced_brackets.push_back(true);
            }
        } else if (line[i] == ']') {
            if (!replaced_brackets.empty()) {
                if (replaced_brackets.back()) {
                    result += '}';
                } else {
                    result += ']';
                }
                replaced_brackets.pop_back();
            } else {
                result += ']';
            }
        } else {
            result += line[i];
        }
    }
    return result;
}

std::string translateLambdas(std::string line) {
    std::regex lambda_regex(R"(fn\s*\(([^)]*)\)\s*\{)");
    std::smatch match;
    while (std::regex_search(line, match, lambda_regex)) {
        std::string args = match[1].str();
        std::string typed_args = formatArgs(args, "var ");
        std::string replacement = "[=](" + typed_args + ") mutable {";
        line = line.replace(match.position(0), match.length(0), replacement);
    }
    return line;
}

std::string translateObjects(std::string line) {
    line = std::regex_replace(line, std::regex(R"(\(\s*\{)"), "(var_object{");
    line = std::regex_replace(line, std::regex(R"(,\s*\{)"), ", var_object{");
    line = std::regex_replace(line, std::regex(R"(:\s*\{)"), ": var_object{");
    line = std::regex_replace(line, std::regex(R"(=\s*\{)"), "= var_object{");

    std::string trimmed = trim(line);
    if (trimmed == "{" || trimmed == "{,") {
        return "var_object{";
    }

    std::regex multiline_pair_regex(R"raw(^\s*"([^"]*)"\s*:\s*(.*)$)raw");
    std::smatch match;
    if (std::regex_match(line, match, multiline_pair_regex)) {
        std::string key = match[1].str();
        std::string val = match[2].str();
        std::string comma = "";
        if (!val.empty() && val.back() == ',') {
            val.pop_back();
            val = trim(val);
            comma = ",";
        }
        return "{\"" + key + "\", " + val + "}" + comma;
    }

    if (line.find('{') != std::string::npos && line.find('}') != std::string::npos) {
        std::regex pair_regex(R"raw("([^"]*)"\s*:\s*([^,}]+))raw");
        line = std::regex_replace(line, pair_regex, R"raw({"$1", $2})raw");
        line = std::regex_replace(line, std::regex(R"(\{\s*\{)"), "var_object{ {");
    }
    return line;
}
