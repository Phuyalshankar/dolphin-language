#include "generator.hpp"
#include "utils.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>
#include <regex>

void transpileFile(const std::string& filepath,
                   std::stringstream& global_stream,
                   std::stringstream& main_stream,
                   std::vector<Scope>& scope_stack,
                   std::set<std::string>& imported_files) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        std::cerr << "Warning: Could not open imported file: " << filepath << std::endl;
        return;
    }

    std::string line;
    bool in_function = false;
    int func_brace_level = 0;
    std::vector<bool> replaced_brackets;

    auto in_initializer = [&]() -> bool {
        if (scope_stack.empty()) return false;
        return scope_stack.back().type == SCOPE_INITIALIZER;
    };

    std::regex fn_regex(R"(^fn\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*\{$)");
    std::regex if_regex(R"(^if\s+(.*)\s*\{$)");
    std::regex elif_regex(R"(^else\s+if\s+(.*)\s*\{$)");
    std::regex while_regex(R"(^while\s+(.*)\s*\{$)");
    std::regex loop_inf_regex(R"(^loop\s*\{$)");
    std::regex loop_cond_regex(R"(^loop\s*\((.*)\)\s*\{$)");
    std::regex assign_regex(R"(^([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(.*)$)");
    std::regex import_regex(R"raw(^import\s+"([^"]+)"\s*$)raw");

    std::string parent_dir = getDirectoryPath(filepath);

    while (std::getline(infile, line)) {
        CodeAndComment cc = splitComment(line);
        std::string trimmed_code = trim(cc.code);
        std::string trimmed_comment = trim(cc.comment);

        if (trimmed_code.empty() && trimmed_comment.empty()) {
            if (in_function) global_stream << "\n";
            else main_stream << "\n";
            continue;
        }

        if (trimmed_code.empty() && !trimmed_comment.empty()) {
            std::string comment_line = "// " + trimmed_comment;
            if (in_function) global_stream << comment_line << "\n";
            else main_stream << comment_line << "\n";
            continue;
        }

        std::string trimmed = trimmed_code;
        bool is_lambda = (trimmed.find("fn ") != std::string::npos || trimmed.find("fn(") != std::string::npos);

        // Import handling
        std::smatch import_match;
        if (std::regex_match(trimmed, import_match, import_regex)) {
            std::string module_name = import_match[1].str();
            std::string import_filename = module_name;
            if (import_filename.rfind(".dolphin") == std::string::npos) {
                import_filename += ".dolphin";
            }
            std::string full_import_path = parent_dir + import_filename;

            if (imported_files.count(full_import_path) == 0) {
                imported_files.insert(full_import_path);
                transpileFile(full_import_path, global_stream, main_stream, scope_stack, imported_files);
            }
            continue;
        }

        // Translate array literals first
        trimmed = translateArrayLiterals(trimmed, replaced_brackets);

        // Translate lambdas
        trimmed = translateLambdas(trimmed);

        // Translate objects
        trimmed = translateObjects(trimmed);

        // Translate .length to .length()
        trimmed = replaceOutsideStrings(trimmed, std::regex(R"(\.length\b(?!\s*\())"), ".length()");

        // Translate INPUT and OUTPUT to PIN_INPUT and PIN_OUTPUT
        trimmed = replaceOutsideStrings(trimmed, std::regex(R"(\bINPUT\b)"), "PIN_INPUT");
        trimmed = replaceOutsideStrings(trimmed, std::regex(R"(\bOUTPUT\b)"), "PIN_OUTPUT");

        // Translate exponentiation ** to Math.pow(base, exp)
        trimmed = replaceOutsideStrings(trimmed, std::regex(R"(([a-zA-Z0-9_\[\].\"'-]+)\s*\*\*\s*([a-zA-Z0-9_\[\].\"'-]+))"), "Math.pow($1, $2)");

        std::string translated = trimmed;
        bool ends_with_brace = (trimmed.back() == '{' || trimmed.back() == '[');
        bool starts_with_close_brace = (trimmed[0] == '}' || trimmed[0] == ']');

        std::smatch match;

        // Parse structures
        if (std::regex_match(trimmed, match, fn_regex)) {
            std::string fn_name = match[1].str();
            std::string args = match[2].str();
            std::string formatted_args = formatArgs(args, "var ");
            
            translated = "var " + fn_name + "(" + formatted_args + ") {";
            
            in_function = true;
            func_brace_level = 1;
            
            scope_stack.push_back({ SCOPE_BLOCK, std::set<std::string>() });
            
            std::stringstream ss(args);
            std::string arg;
            while (std::getline(ss, arg, ',')) {
                scope_stack.back().vars.insert(trim(arg));
            }
            
            if (!trimmed_comment.empty()) {
                translated += " // " + trimmed_comment;
            }
            global_stream << translated << "\n";
            continue;
        }

        // Track braces to handle scope entry/exit
        if (ends_with_brace && !std::regex_match(trimmed, match, fn_regex)) {
            if (in_function) {
                func_brace_level++;
            }
            ScopeType st = SCOPE_BLOCK;
            if (in_initializer()) {
                st = SCOPE_INITIALIZER;
            } else {
                bool is_structural = std::regex_match(trimmed, if_regex) ||
                                     std::regex_match(trimmed, elif_regex) ||
                                     std::regex_match(trimmed, while_regex) ||
                                     std::regex_match(trimmed, loop_inf_regex) ||
                                     std::regex_match(trimmed, loop_cond_regex) ||
                                     trimmed == "else {";
                if (!is_structural && !is_lambda) {
                    st = SCOPE_INITIALIZER;
                }
            }
            scope_stack.push_back({ st, std::set<std::string>() });
        }

        // Handle structural statement translations
        if (std::regex_match(trimmed, match, if_regex)) {
            translated = "if (" + match[1].str() + ") {";
        } else if (std::regex_match(trimmed, match, elif_regex)) {
            translated = "else if (" + match[1].str() + ") {";
        } else if (std::regex_match(trimmed, match, while_regex)) {
            translated = "while (" + match[1].str() + ") {";
        } else if (std::regex_match(trimmed, loop_inf_regex)) {
            translated = "while (true) {";
        } else if (std::regex_match(trimmed, match, loop_cond_regex)) {
            std::string inner = match[1].str();
            
            // Check if it's a range loop: e.g. i, 0, 10
            std::regex range_regex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*(.*?)\s*,\s*(.*?)\s*$)");
            std::smatch range_match;
            
            // Check if it's a foreach loop: e.g. item in items
            std::regex in_regex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s+in\s+(.*?)\s*$)");
            std::smatch in_match;
            
            if (std::regex_match(inner, range_match, range_regex)) {
                std::string var_name = range_match[1].str();
                std::string start = range_match[2].str();
                std::string end = range_match[3].str();
                translated = "for (var " + var_name + " = " + start + "; " + var_name + " < " + end + "; " + var_name + "++) {";
            } else if (std::regex_match(inner, in_match, in_regex)) {
                std::string var_name = in_match[1].str();
                std::string container = in_match[2].str();
                translated = "for (auto& " + var_name + " : " + container + ") {";
            } else {
                translated = "while (" + inner + ") {";
            }
        } else if (trimmed == "else {") {
            translated = "else {";
        } else if (trimmed.rfind("return", 0) == 0) {
            std::string ret_val = trim(trimmed.substr(6));
            if (ret_val.empty() || ret_val == ";") {
                translated = "return var();";
            } else {
                if (ret_val.back() != ';') ret_val += ";";
                translated = "return " + ret_val;
            }
        } else if (starts_with_close_brace) {
            translated = "}";
            if (trimmed.length() > 1) {
                translated = trimmed;
            }
            
            bool popped_initializer = false;
            if (scope_stack.size() > 1) {
                if (scope_stack.back().type == SCOPE_INITIALIZER) {
                    popped_initializer = true;
                }
                scope_stack.pop_back();
            }
            
            if (in_function) {
                func_brace_level--;
                if (func_brace_level == 0) {
                    in_function = false;
                    global_stream << "return var();\n";
                    if (!trimmed_comment.empty()) {
                        translated += " // " + trimmed_comment;
                    }
                    global_stream << translated << "\n";
                    continue;
                }
            }

            if (translated.back() == ')') {
                translated += ";";
            } else if (popped_initializer) {
                if (translated.back() != ';' && translated.back() != ',' && !in_initializer()) {
                    translated += ";";
                }
            }
        } else if (std::regex_match(trimmed, match, assign_regex)) {
            std::string var_name = match[1].str();
            std::string expr = match[2].str();

            bool declared = false;
            for (const auto& scope : scope_stack) {
                if (scope.vars.count(var_name)) {
                    declared = true;
                    break;
                }
            }

            if (expr == "{") {
                expr = "var_object{";
            }

            if (!declared) {
                if (ends_with_brace && scope_stack.size() > 1) {
                    scope_stack[scope_stack.size() - 2].vars.insert(var_name);
                } else {
                    scope_stack.back().vars.insert(var_name);
                }
                if (scope_stack.size() == 1 && !in_function) {
                    if (expr.rfind("pin(", 0) == 0) {
                        global_stream << "pin " << var_name << ";\n";
                    } else {
                        global_stream << "var " << var_name << ";\n";
                    }
                    translated = var_name + " = " + expr;
                } else {
                    if (expr.rfind("pin(", 0) == 0) {
                        translated = "pin " + var_name + " = " + expr;
                    } else {
                        translated = "var " + var_name + " = " + expr;
                    }
                }
            } else {
                translated = var_name + " = " + expr;
            }

            if (!in_initializer() && translated.back() != ';' && translated.back() != '{' && translated.back() != '(' && translated.back() != ',') {
                translated += ";";
            }
        } else {
            if (!in_initializer() && translated.back() != ';' && translated.back() != ',' && translated.back() != '(' && !ends_with_brace && !starts_with_close_brace) {
                translated += ";";
            }
        }

        // Add inline comment if present
        if (!trimmed_comment.empty()) {
            translated += " // " + trimmed_comment;
        }

        // Output to appropriate stream
        if (in_function) {
            global_stream << translated << "\n";
        } else {
            main_stream << translated << "\n";
        }
    }
}
