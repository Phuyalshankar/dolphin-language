#include "ast.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <sstream>
#include <stdexcept>
#include <regex>
#include <iostream>
#include <unordered_set>

// ─── Forward declarations ─────────────────────────────────────────────────────
static std::string compileSelf(const std::string& raw, const std::string& current_class);

// ─── Helper: compileArgs ─────────────────────────────────────────────────────
static std::string compileArgs(const std::vector<std::unique_ptr<Expr>>& args, CodegenContext& ctx) {
    std::string result = "std::vector<var>{";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) result += ", ";
        result += args[i]->compile(ctx);
    }
    return result + "}";
}

static std::string compileArgsComma(const std::vector<std::unique_ptr<Expr>>& args, CodegenContext& ctx) {
    std::string result = "";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) result += ", ";
        result += args[i]->compile(ctx);
    }
    return result;
}

// ─── LiteralExpr ─────────────────────────────────────────────────────────────
std::string LiteralExpr::compile(CodegenContext& ctx) const {
    switch (kind) {
        case LIT_NULL:    return "var()";
        case LIT_BOOL:    return value == "true" ? "var(true)" : "var(false)";
        case LIT_INT:     return "var(" + value + ")";
        case LIT_DOUBLE:  return "var(" + value + ")";
        case LIT_STRING:  {
            // value is already "\"...\""
            std::string escaped = "";
            for (char c : value) {
                if (c == '\n') {
                    escaped += "\\n";
                } else if (c == '\r') {
                    escaped += "\\r";
                } else {
                    escaped += c;
                }
            }
            return "var(" + escaped + ")";
        }
        case LIT_TEMPLATE: {
            // Convert `hello ${name}!` to C++ string concatenation
            // Template literal is stored as "`...`"
            std::string raw = value.substr(1, value.length() - 2); // strip backticks
            std::string result = "var(std::string(\"\")";
            size_t i = 0;
            std::string buf;
            while (i < raw.size()) {
                if (raw[i] == '$' && i + 1 < raw.size() && raw[i+1] == '{') {
                    if (!buf.empty()) {
                        result += " + var(\"" + buf + "\")";
                        buf.clear();
                    }
                    i += 2; // skip ${
                    std::string expr_str;
                    int depth = 1;
                    while (i < raw.size() && depth > 0) {
                        if (raw[i] == '{') depth++;
                        else if (raw[i] == '}') { depth--; if (depth == 0) break; }
                        expr_str += raw[i++];
                    }
                    if (i < raw.size()) i++; // skip }
                    result += " + var(" + expr_str + ")";
                } else if (raw[i] == '\\' && i + 1 < raw.size()) {
                    char esc = raw[i+1];
                    switch (esc) {
                        case 'n':  buf += "\\n"; break;
                        case 't':  buf += "\\t"; break;
                        case 'r':  buf += "\\r"; break;
                        default:   buf += '\\'; buf += esc; break;
                    }
                    i += 2;
                } else {
                    // Escape double quotes and newlines inside C++ string
                    if (raw[i] == '"') buf += "\\\"";
                    else if (raw[i] == '\n') buf += "\\n";
                    else if (raw[i] == '\r') buf += "\\r";
                    else buf += raw[i];
                    i++;
                }
            }
            if (!buf.empty()) result += " + var(\"" + buf + "\")";
            result += ")";
            return result;
        }
    }
    return "var()";
}

// ─── IdentifierExpr ───────────────────────────────────────────────────────────
std::string IdentifierExpr::compile(CodegenContext& ctx) const {
    // Map IoT namespaces + builtins
    if (name == "print")       return "dolphin_print";
    if (name == "println")     return "dolphin_println";
    if (name == "input")       return "dolphin_input";
    if (name == "len")         return "dolphin_len";
    if (name == "range")       return "dolphin_range";
    if (name == "type")        return "dolphin_typeof";
    if (name == "parseInt")    return "dolphin_parseInt";
    if (name == "parseFloat")  return "dolphin_parseFloat";
    if (name == "str")         return "dolphin_str";
    if (name == "int")         return "dolphin_int";
    if (name == "float")       return "dolphin_float";
    if (name == "Math")        return "DolphinMath";
    if (name == "JSON")        return "DolphinJSON";
    if (name == "File")        return "DolphinFile";
    if (name == "Date")        return "DolphinDate";
    if (name == "Random")      return "DolphinRandom";
    if (name == "Process")     return "DolphinProcess";
    if (name == "Dolphin")     return "DolphinRuntime";
    if (name == "Wifi")        return "DolphinWifi";
    if (name == "Bluetooth")   return "DolphinBT";
    if (name == "GPIO")        return "DolphinGPIO";
    if (name == "Http")        return "DolphinHttp";
    if (name == "Modbus")      return "DolphinModbus";
    if (name == "CAN")         return "DolphinCAN";
    if (name == "Zigbee")      return "DolphinZigbee";
    if (name == "HL7")         return "DolphinHL7";
    if (name == "OUTPUT")      return "DOLPHIN_OUTPUT";
    if (name == "INPUT")       return "DOLPHIN_INPUT";
    if (name == "HIGH")        return "DOLPHIN_HIGH";
    if (name == "LOW")         return "DOLPHIN_LOW";
    if (name == "self")        return "__self__";
    if (name == "__super__")   return "__super_class__";  // parent factory for super() calls
    return name;
}

// ─── BinaryExpr ──────────────────────────────────────────────────────────────
std::string BinaryExpr::compile(CodegenContext& ctx) const {
    if (op == ".") {
        std::string lhs_str = lhs->compile(ctx);
        auto id = dynamic_cast<IdentifierExpr*>(rhs.get());
        if (id) {
            std::string name = id->name;
            auto lhs_id = dynamic_cast<IdentifierExpr*>(lhs.get());
            if (lhs_str == "WiFi" || lhs_str == "Bluetooth" || lhs_str == "Zigbee" || 
                lhs_str == "CAN" || lhs_str == "Modbus" || lhs_str == "HL7" || 
                lhs_str == "HTTP" || lhs_str == "TCP" || lhs_str == "File" || lhs_str == "Dolphin" || lhs_str == "GPIO" || lhs_str == "Template" ||
                lhs_str == "Matrix" || lhs_str == "ML") {
                return lhs_str + "." + name;
            }
            if (lhs_id && ctx.getDeclaredType(lhs_id->name) == "pin") {
                return lhs_str + "." + name;
            }
            static const std::unordered_set<std::string> native_var_methods = {
                "on", "emit", "once", "off", "listen", "get", "post",
                "push", "add", "size", "has", "trim", "isEven", "isOdd", "keys",
                "values", "entries", "filter", "map", "forEach", "toInt", "toDouble", "toString",
                "transpose", "matmul", "sigmoid", "sigmoidDerivative", "relu", "reluDerivative", "toArray",
                "close", "status", "send", "json"
            };
            if (native_var_methods.count(name)) {
                return lhs_str + "." + name;
            } else {
                return lhs_str + "[\"" + name + "\"]";
            }
        }
        return lhs_str + "." + rhs->compile(ctx);
    }

    std::string l = lhs->compile(ctx);
    std::string r = rhs->compile(ctx);

    // Exponent
    if (op == "**") return "dolphin_pow(" + l + ", " + r + ")";

    // Bitwise / logical operators
    if (op == "&")  return "(" + l + " & " + r + ")";
    if (op == "|")  return "(" + l + " | " + r + ")";
    if (op == "^")  return "(" + l + " ^ " + r + ")";
    if (op == "<<") return "(" + l + " << " + r + ")";
    if (op == ">>") return "(" + l + " >> " + r + ")";

    return "(" + l + " " + op + " " + r + ")";
}

// ─── UnaryExpr ────────────────────────────────────────────────────────────────
std::string UnaryExpr::compile(CodegenContext& ctx) const {
    std::string operand = rhs->compile(ctx);
    if (postfix) return "(" + operand + op + ")";
    if (op == "~") return "dolphin_bitnot(" + operand + ")";
    return "(" + op + operand + ")";
}

// ─── TypeofExpr ──────────────────────────────────────────────────────────────
std::string TypeofExpr::compile(CodegenContext& ctx) const {
    return "dolphin_typeof(" + expr->compile(ctx) + ")";
}

// ─── NewExpr ─────────────────────────────────────────────────────────────────
std::string NewExpr::compile(CodegenContext& ctx) const {
    std::string result = class_name + "(" + compileArgs(args, ctx) + ")";
    return result;
}

// ─── AwaitExpr ────────────────────────────────────────────────────────────────
std::string AwaitExpr::compile(CodegenContext& ctx) const {
    return "co_await " + expr->compile(ctx);
}

// ─── CallExpr ────────────────────────────────────────────────────────────────
std::string CallExpr::compile(CodegenContext& ctx) const {
    auto* binExpr = dynamic_cast<BinaryExpr*>(callee.get());
    if (binExpr && binExpr->op == ".") {
        // Method call: obj.method(args)
        std::string obj_code = binExpr->lhs->compile(ctx);
        auto* id = dynamic_cast<IdentifierExpr*>(binExpr->rhs.get());
        std::string method_name = id ? id->name : binExpr->rhs->compile(ctx);
        std::string args_code = compileArgs(args, ctx);

        // Check if native namespace
        if (obj_code == "WiFi" || obj_code == "Bluetooth" || obj_code == "Zigbee" || 
            obj_code == "CAN" || obj_code == "Modbus" || obj_code == "HL7" || 
            obj_code == "HTTP" || obj_code == "TCP" || obj_code == "File" || 
            obj_code == "Dolphin" || obj_code == "GPIO" || obj_code == "Template" ||
            obj_code == "Matrix" || obj_code == "ML") {
            return obj_code + "." + method_name + "(" + compileArgsComma(args, ctx) + ")";
        }

        // Check if native var method
        static const std::unordered_set<std::string> native_var_methods = {
            "on", "emit", "once", "off", "listen", "get", "post",
            "push", "add", "size", "has", "trim", "isEven", "isOdd", "keys",
            "values", "entries", "filter", "map", "forEach", "toInt", "toDouble", "toString",
            "transpose", "matmul", "sigmoid", "sigmoidDerivative", "relu", "reluDerivative", "toArray",
            "close", "status", "send", "json"
        };

        if (native_var_methods.count(method_name)) {
            return obj_code + "." + method_name + "(" + compileArgsComma(args, ctx) + ")";
        }

        // Built-in array methods
        static const std::set<std::string> arr_methods = {
            "push","pop","shift","unshift","splice","slice","indexOf",
            "lastIndexOf","includes","reverse","sort","join","flat","map",
            "filter","reduce","forEach","some","every","find","findIndex",
            "concat","fill","copyWithin","entries","keys","values"
        };
        // Built-in string methods
        static const std::set<std::string> str_methods = {
            "split","replace","replaceAll","startsWith","endsWith","includes",
            "indexOf","lastIndexOf","slice","substring","substr","padStart",
            "padEnd","repeat","trim","trimStart","trimEnd","toUpperCase",
            "toLowerCase","charAt","charCodeAt","fromCharCode","at","match",
            "matchAll","search","normalize","localeCompare","concat"
        };

        if (arr_methods.count(method_name) || str_methods.count(method_name)) {
            return obj_code + ".call_method(\"" + method_name + "\", " + args_code + ")";
        }

        // General: obj["method"](args) or obj.call_method(...)
        return obj_code + "[\"" + method_name + "\"](" + args_code + ")";
    }

    std::string callee_code = callee->compile(ctx);
    std::string args_code   = compileArgs(args, ctx);

    // super(args) — call parent factory and copy resulting fields to self
    if (callee_code == "__super_class__") {
        std::string flat;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) flat += ", ";
            flat += args[i]->compile(ctx);
        }
        return "[&]() -> var { "
               "var __sr__ = __super_class__(std::vector<var>{" + flat + "}); "
               "for (auto& __sf__ : __sr__.as_object()) { "
               "    __self__[__sf__.first] = __sf__.second; } "
               "return var(); }()";
    }

    // Some builtins use different call convention
    if (callee_code == "dolphin_print"   ||
        callee_code == "dolphin_println" ||
        callee_code == "dolphin_len"     ||
        callee_code == "dolphin_range"   ||
        callee_code == "dolphin_input"   ||
        callee_code == "dolphin_typeof"  ||
        callee_code == "dolphin_parseInt"||
        callee_code == "dolphin_parseFloat" ||
        callee_code == "dolphin_str"     ||
        callee_code == "dolphin_int"     ||
        callee_code == "dolphin_float"   ||
        callee_code == "dolphin_pow"     ||
        callee_code == "dolphin_bitnot") {
        // These take direct args  
        if (!args.empty()) {
            std::string flat;
            for (size_t i = 0; i < args.size(); ++i) {
                if (i) flat += ", ";
                flat += args[i]->compile(ctx);
            }
            return callee_code + "(" + flat + ")";
        }
        return callee_code + "()";
    }

    return callee_code + "(" + args_code + ")";
}

// ─── IndexExpr ────────────────────────────────────────────────────────────────
std::string IndexExpr::compile(CodegenContext& ctx) const {
    return expr->compile(ctx) + "[" + index->compile(ctx) + "]";
}

// ─── ArrayLiteralExpr ────────────────────────────────────────────────────────
std::string ArrayLiteralExpr::compile(CodegenContext& ctx) const {
    std::string result = "var(var_array{";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) result += ", ";
        result += elements[i]->compile(ctx);
    }
    return result + "})";
}

// ─── ObjectLiteralExpr ───────────────────────────────────────────────────────
std::string ObjectLiteralExpr::compile(CodegenContext& ctx) const {
    std::string result = "var(var_object{{";
    for (size_t i = 0; i < properties.size(); ++i) {
        if (i > 0) result += ", ";
        result += "{\"" + properties[i].first + "\", " + properties[i].second->compile(ctx) + "}";
    }
    return result + "}})";
}

// ─── LambdaExpr ──────────────────────────────────────────────────────────────
std::string LambdaExpr::compile(CodegenContext& ctx) const {
    std::stringstream ss;
    ss << "var([=](const std::vector<var>& __args__) mutable -> var {\n";

    // Declare parameters from __args__
    for (size_t i = 0; i < args.size(); ++i) {
        ss << "    var " << args[i] << " = __args__.size() > " << i << " ? __args__[" << i << "] : var();\n";
    }
    
    if (is_async) {
        ctx.indent(ss);
        ss << "var _promise = var::Promise();\n";
        ctx.indent(ss);
        ss << "Dolphin.async([=]() mutable {\n";
        
        ctx.indent_level += 2;
        ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, std::set<std::string>() });
        ctx.typed_scope_stack.push_back({});
        bool old_in_async = ctx.in_async_func;
        ctx.in_async_func = true;
        
        auto block_body = dynamic_cast<BlockStmt*>(body.get());
        if (block_body) {
            for (auto const& stmt : block_body->statements) {
                if (!stmt) continue;
                ctx.indent(ss);
                ss << stmt->compile(ctx);
            }
        } else {
            ctx.indent(ss);
            ss << body->compile(ctx);
        }
        
        ctx.in_async_func = old_in_async;
        ctx.scope_stack.pop_back();
        ctx.typed_scope_stack.pop_back();
        ctx.indent(ss);
        ss << "_promise.resolve(var());\n";
        ctx.indent_level -= 2;
        
        ctx.indent(ss);
        ss << "});\n";
        ctx.indent(ss);
        ss << "return _promise;\n";
        ss << "})";
    } else {
        bool old_in_func = ctx.in_function;
        ctx.in_function = true;
        ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, std::set<std::string>() });
        ctx.typed_scope_stack.push_back({});

        auto block_body = dynamic_cast<BlockStmt*>(body.get());
        if (block_body) {
            for (auto const& stmt : block_body->statements) {
                if (!stmt) continue;
                ctx.indent(ss);
                ss << stmt->compile(ctx);
            }
            ctx.indent(ss);
            ss << "return var();\n";
        } else {
            ctx.indent(ss);
            ss << body->compile(ctx);
        }

        ctx.scope_stack.pop_back();
        ctx.typed_scope_stack.pop_back();
        ctx.in_function = old_in_func;
        ss << "})";
    }
    return ss.str();
}

// ─── MatchExpr ───────────────────────────────────────────────────────────────
std::string MatchExpr::compile(CodegenContext& ctx) const {
    std::string v = val->compile(ctx);
    std::string tmp = "__match_val__";
    std::string result = "[&]() -> var { var " + tmp + " = " + v + ";";

    for (auto& arm : arms) {
        result += " if (" + tmp + " == " + arm.first->compile(ctx) + ") return " + arm.second->compile(ctx) + ";";
    }
    if (default_arm) {
        result += " return " + default_arm->compile(ctx) + ";";
    } else {
        result += " return var();";
    }
    result += " }()";
    return result;
}

// ─── ListComprehensionExpr ───────────────────────────────────────────────────
std::string ListComprehensionExpr::compile(CodegenContext& ctx) const {
    std::string tmp_arr = "__comp_result__";
    std::string tmp_el  = var_name;
    std::string coll    = container->compile(ctx);
    std::string body_e  = expr->compile(ctx);

    std::string result = "[&]() { var " + tmp_arr + " = var(var_array{});";
    result += " for (auto& " + tmp_el + " : " + coll + ".as_array()) {";
    if (condition) {
        result += " if (" + condition->compile(ctx) + ".as_bool()) {";
        result += " " + tmp_arr + ".push_back(" + body_e + ");";
        result += " }";
    } else {
        result += " " + tmp_arr + ".push_back(" + body_e + ");";
    }
    result += " } return " + tmp_arr + "; }()";
    return result;
}

// ─── BlockStmt ───────────────────────────────────────────────────────────────
std::string BlockStmt::compile(CodegenContext& ctx) const {
    std::stringstream ss;
    ss << "{\n";
    ctx.indent_level++;
    ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, {}, "" });
    ctx.typed_scope_stack.push_back({});

    for (auto const& stmt : statements) {
        if (!stmt) continue;
        ctx.indent(ss);
        ss << stmt->compile(ctx);
    }

    ctx.scope_stack.pop_back();
    ctx.typed_scope_stack.pop_back();
    ctx.indent_level--;
    ctx.indent(ss);
    ss << "}\n";
    return ss.str();
}

// ─── ExprStmt ────────────────────────────────────────────────────────────────
std::string ExprStmt::compile(CodegenContext& ctx) const {
    return expr->compile(ctx) + ";";
}

// ─── VarDeclStmt ─────────────────────────────────────────────────────────────
std::string VarDeclStmt::compile(CodegenContext& ctx) const {
    ctx.declare(name);
    std::string val = initializer ? initializer->compile(ctx) : "var()";
    if (ctx.hardware_target && ctx.scope_stack.size() == 1) {
        ctx.global_stream << "var " << name << ";\n";
        return name + " = " + val + ";";
    }
    return "var " + name + " = " + val + ";";
}

std::string TypedDeclStmt::compile(CodegenContext& ctx) const {
    ctx.declareTyped(name, type_name);
    std::string init = initializer ? initializer->compile(ctx) : type_name + "()";
    if (ctx.hardware_target && ctx.scope_stack.size() == 1) {
        ctx.global_stream << type_name << " " << name << ";\n";
        return name + " = " + init + ";";
    }
    return type_name + " " + name + " = " + init + ";\n";
}

// ─── AssignStmt ──────────────────────────────────────────────────────────────
std::string AssignStmt::compile(CodegenContext& ctx) const {
    std::string l = lhs->compile(ctx);
    std::string r = rhs->compile(ctx);

    // Check if lhs is an identifier that has been declared
    auto* id = dynamic_cast<IdentifierExpr*>(lhs.get());
    bool declared = id && ctx.isDeclared(id->name);

    // Also check for self.field assignments (always treated as assignment, not decl)
    bool is_member_access = (l.find('[') != std::string::npos);

    if (!declared && !is_member_access && id && id->name != "self") {
        ctx.declare(id->name);
        if (op == "=") return "var " + l + " = " + r + ";";
        return "var " + l + " = " + l + " " + op.substr(0, op.size()-1) + " " + r + ";";
    }

    if (op == "=") return l + " = " + r + ";";
    return l + " " + op + " " + r + ";";
}

// ─── FuncDeclStmt ─────────────────────────────────────────────────────────────
std::string FuncDeclStmt::compile(CodegenContext& ctx) const {
    std::stringstream ss;
    bool old_fn = ctx.in_function;
    ctx.in_function = true;

    if (is_async) {
        ss << "var " << name << " = var([=](const std::vector<var>& __args__) mutable -> var {\n";
        ss << "    var _promise = var::Promise();\n";
        ss << "    Dolphin.async([=]() mutable {\n";
        ctx.indent_level += 2;
        ctx.scope_stack.push_back({ Scope::SCOPE_INITIALIZER, {}, "" });
        ctx.typed_scope_stack.push_back({});
        bool old_in_async = ctx.in_async_func;
        ctx.in_async_func = true;
        for (size_t i = 0; i < args.size(); ++i) {
            ctx.declare(args[i]);
            ss << "        var " << args[i] << " = __args__.size() > " << i << " ? __args__[" << i << "] : var();\n";
        }
        ss << body->compile(ctx);
        ctx.in_async_func = old_in_async;
        ctx.scope_stack.pop_back();
        ctx.typed_scope_stack.pop_back();
        ss << "        _promise.resolve(var());\n";
        ctx.indent_level -= 2;
        ss << "    });\n";
        ss << "    return _promise;\n";
        ss << "});";
    } else {
        ss << "var " << name << " = var([=](const std::vector<var>& __args__) -> var {\n";
        ctx.scope_stack.push_back({ Scope::SCOPE_INITIALIZER, {}, "" });
        ctx.typed_scope_stack.push_back({});
        for (size_t i = 0; i < args.size(); ++i) {
            ctx.declare(args[i]);
            ss << "    var " << args[i] << " = __args__.size() > " << i << " ? __args__[" << i << "] : var();\n";
        }
        ss << body->compile(ctx);
        ctx.scope_stack.pop_back();
        ctx.typed_scope_stack.pop_back();
        ss << "    return var();\n});";
    }

    ctx.in_function = old_fn;
    ctx.declare(name);
    return ss.str();
}

// ─── IfStmt ──────────────────────────────────────────────────────────────────
std::string IfStmt::compile(CodegenContext& ctx) const {
    std::string result = "if ((" + condition->compile(ctx) + ").as_bool()) {\n";
    ctx.indent_level++;
    result += then_branch->compile(ctx);
    ctx.indent_level--;
    result += "}";
    if (else_branch) {
        result += " else {\n";
        ctx.indent_level++;
        result += else_branch->compile(ctx);
        ctx.indent_level--;
        result += "}";
    }
    return result;
}

// ─── WhileStmt ───────────────────────────────────────────────────────────────
std::string WhileStmt::compile(CodegenContext& ctx) const {
    std::string result = "while ((" + condition->compile(ctx) + ").as_bool()) {\n";
    ctx.indent_level++;
    result += body->compile(ctx);
    ctx.indent_level--;
    result += "}";
    return result;
}

// ─── ForRangeStmt ────────────────────────────────────────────────────────────
std::string ForRangeStmt::compile(CodegenContext& ctx) const {
    std::string s  = start->compile(ctx);
    std::string e  = end->compile(ctx);
    std::string result = "for (var " + var_name + " = " + s + "; " +
                         "(" + var_name + " < " + e + ").as_bool(); ++" + var_name + ") {\n";
    ctx.declare(var_name);
    ctx.indent_level++;
    result += body->compile(ctx);
    ctx.indent_level--;
    result += "}";
    return result;
}

// ─── ForEachStmt ─────────────────────────────────────────────────────────────
std::string ForEachStmt::compile(CodegenContext& ctx) const {
    std::string coll = container->compile(ctx);
    std::string result = "for (auto& __raw_" + var_name + " : (" + coll + ").as_iterable()) {\n";
    ctx.declare(var_name);
    result += "    var " + var_name + " = __raw_" + var_name + ";\n";
    ctx.indent_level++;
    result += body->compile(ctx);
    ctx.indent_level--;
    result += "}";
    return result;
}

// ─── InfiniteLoopStmt ────────────────────────────────────────────────────────
std::string InfiniteLoopStmt::compile(CodegenContext& ctx) const {
    std::string result = "while (true) {\n";
    ctx.indent_level++;
    result += body->compile(ctx);
    ctx.indent_level--;
    result += "}";
    return result;
}

// ─── ReturnStmt ──────────────────────────────────────────────────────────────
std::string ReturnStmt::compile(CodegenContext& ctx) const {
    if (value) return "return " + value->compile(ctx) + ";";
    return "return var();";
}

// ─── BreakStmt ───────────────────────────────────────────────────────────────
std::string BreakStmt::compile(CodegenContext& ctx) const {
    return "break;";
}

// ─── ContinueStmt ────────────────────────────────────────────────────────────
std::string ContinueStmt::compile(CodegenContext& ctx) const {
    return "continue;";
}

// ─── ImportStmt ──────────────────────────────────────────────────────────────
std::string ImportStmt::compile(CodegenContext& ctx) const {
    std::stringstream ss;
    for (auto& stmt : resolved_stmts) {
        ctx.indent(ss);
        ss << stmt->compile(ctx) << "\n";
    }
    return ss.str();
}

// ─── TryCatchStmt ────────────────────────────────────────────────────────────
std::string TryCatchStmt::compile(CodegenContext& ctx) const {
    std::string result = "try {\n";
    ctx.indent_level++;
    result += try_block->compile(ctx);
    ctx.indent_level--;
    result += "}";

    if (catch_block) {
        result += " catch (const var& " + catch_var + ") {\n";
        ctx.indent_level++;
        ctx.declare(catch_var);
        result += catch_block->compile(ctx);
        ctx.indent_level--;
        result += "} catch (const std::exception& __ex__) {\n";
        result += "    var " + catch_var + " = var(std::string(__ex__.what()));\n";
        ctx.indent_level++;
        result += catch_block->compile(ctx);
        ctx.indent_level--;
        result += "} catch (...) {\n";
        result += "    var " + catch_var + " = var(\"unknown exception\");\n";
        ctx.indent_level++;
        result += catch_block->compile(ctx);
        ctx.indent_level--;
        result += "}";
    }

    if (finally_block) {
        // Simulate finally using RAII
        result += "\n// finally\n{\n";
        ctx.indent_level++;
        result += finally_block->compile(ctx);
        ctx.indent_level--;
        result += "}";
    }

    return result;
}

// ─── ThrowStmt ───────────────────────────────────────────────────────────────
std::string ThrowStmt::compile(CodegenContext& ctx) const {
    return "throw " + expr->compile(ctx) + ";";
}

// ─── ClassDeclStmt ───────────────────────────────────────────────────────────
// Compiles class declarations to factory functions using the var_object runtime.
//
// class Dog extends Animal {
//     fn init(name) { self.name = name }
//     fn bark() { print(self.name + " barks!") }
// }
// →
// var Dog = var([=](const std::vector<var>& __ctor_args__) mutable -> var {
//     var __self__ = var(var_object{});
//     // Inherited methods from Animal
//     var __parent__ = Animal.__proto__;
//     // Define methods as closures capturing self by reference
//     var __init__ = var([&__self__](const std::vector<var>& __args__) mutable -> var {
//         var name = __args__[0];
//         __self__["name"] = name;
//         return var();
//     });
//     __self__["init"] = __init__;
//     __self__["bark"] = var([&__self__](const std::vector<var>& __args__) mutable -> var {
//         dolphin_print(__self__["name"] + var(" barks!"));
//         return var();
//     });
//     if (__self__["init"].is_callable()) __self__["init"](__ctor_args__);
//     return __self__;
// });
//
std::string ClassDeclStmt::compile(CodegenContext& ctx) const {
    std::stringstream ss;

    // Class factory lambda ─────────────────────────────────────────────────
    // var_object is stored via shared_ptr<var_object>, so all copies of a
    // var that holds an object share the same underlying map.  We capture
    // __self__ BY VALUE in every method closure — safe, and mutations are
    // shared because the shared_ptr is shared.
    //
    // super(args) is compiled to:
    //   { var __s__ = __super_class__(args);
    //     for each field in __s__: __self__[field] = __s__[field]; }
    // so child fields/methods override after the call.
    // ──────────────────────────────────────────────────────────────────────
    ss << "var " << name << " = var([=](const std::vector<var>& __ctor_args__) mutable -> var {\n";
    ss << "    var __self__ = var(var_object{});\n";

    // Inheritance: copy parent prototype methods onto __self__ first
    if (!base_class.empty()) {
        ss << "    // Inherit from " << base_class << "\n";
        ss << "    {\n";
        ss << "        var __base__ = " << base_class << "(std::vector<var>{});\n";
        ss << "        for (auto& __kv__ : __base__.as_object()) {\n";
        ss << "            __self__[__kv__.first] = __kv__.second;\n";
        ss << "        }\n";
        ss << "    }\n";
        // __super_class__ is available inside every method for super() calls
        ss << "    var __super_class__ = " << base_class << ";\n";
    }

    // Generate each method (value-capture of __self__; safe via shared_ptr)
    for (auto& method : methods) {
        ss << "\n    // Method: " << method.name << "\n";
        // Capture __self__ by value (shared_ptr ensures shared state)
        // Also capture __super_class__ if we have a base class
        std::string capture = "__self_ref__ = __self__";
        if (!base_class.empty()) capture += ", __super_class__";
        ss << "    __self__[\"" << method.name << "\"] = var(["
           << capture << "](const std::vector<var>& __args__) mutable -> var {\n";
        // Re-bind __self__ so generated code can use it by name
        ss << "        var __self__ = __self_ref__;\n";

        // Parameter bindings
        for (size_t i = 0; i < method.params.size(); ++i) {
            ss << "        var " << method.params[i] << " = __args__.size() > " << i
               << " ? __args__[" << i << "] : var();\n";
        }

        // Compile body with class context
        CodegenContext method_ctx;
        method_ctx.scope_stack.push_back({ Scope::SCOPE_CLASS, {}, name });
        method_ctx.in_function   = true;
        method_ctx.in_class      = true;
        method_ctx.current_class = name;
        method_ctx.indent_level  = 2;
        for (auto& p : method.params) method_ctx.declare(p);

        std::string body_code = method.body->compile(method_ctx);
        ss << body_code;
        ss << "        return var();\n";
        ss << "    });\n";
    }

    // Call constructor
    ss << "\n    if (__self__.has_key(\"init\")) {\n";
    ss << "        __self__[\"init\"](__ctor_args__);\n";
    ss << "    }\n";
    ss << "    return __self__;\n";
    ss << "});";

    ctx.declare(name);
    return ss.str();
}

// ─── Top-level code generation ────────────────────────────────────────────────
std::string generateCode(const std::unique_ptr<BlockStmt>& ast) {
    CodegenContext ctx;
    ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, {}, "" });

    std::stringstream ss;
    ss << "// Generated by Dolphin transpiler\n";
    ss << "#include \"dolphin_runtime.hpp\"\n\n";
    ss << "int main() {\n";
    ss << "    dolphin_init();\n";

    for (auto& stmt : ast->statements) {
        ss << "    ";
        ss << stmt->compile(ctx);
        ss << "\n";
    }

    ss << "    return 0;\n";
    ss << "}\n";

    return ss.str();
}

std::string generateHardwareCode(const std::unique_ptr<BlockStmt>& ast) {
    CodegenContext ctx;
    ctx.hardware_target = true;
    ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, {}, "" });

    std::stringstream global_ss;
    std::stringstream setup_ss;
    std::stringstream loop_ss;

    // Find the first top-level infinite loop block
    const Stmt* loopBlock = nullptr;
    const Stmt* loopStmt = nullptr;
    
    for (const auto& stmt : ast->statements) {
        if (auto* loop = dynamic_cast<const InfiniteLoopStmt*>(stmt.get())) {
            loopBlock = loop->body.get();
            loopStmt = stmt.get();
            break;
        }
    }

    // First pass: function and class declarations go to global
    for (const auto& stmt : ast->statements) {
        if (dynamic_cast<const FuncDeclStmt*>(stmt.get()) || dynamic_cast<const ClassDeclStmt*>(stmt.get())) {
            global_ss << stmt->compile(ctx) << "\n";
        }
    }
    // Second pass: variables and other setups go to setup()
    for (const auto& stmt : ast->statements) {
        if (dynamic_cast<const FuncDeclStmt*>(stmt.get()) || dynamic_cast<const ClassDeclStmt*>(stmt.get())) {
            continue;
        }
        if (stmt.get() == loopStmt) {
            continue;
        }
        std::string compiled = stmt->compile(ctx);
        if (!compiled.empty()) {
            setup_ss << "    " << compiled << "\n";
        }
    }

    // Third pass: loop body goes to loop()
    if (loopBlock) {
        // If the loop body is a block statement, compile its inner statements, else compile the statement itself
        if (auto* block = dynamic_cast<const BlockStmt*>(loopBlock)) {
            for (const auto& stmt : block->statements) {
                std::string compiled = stmt->compile(ctx);
                if (!compiled.empty()) {
                    loop_ss << "    " << compiled << "\n";
                }
            }
        } else {
            std::string compiled = loopBlock->compile(ctx);
            if (!compiled.empty()) {
                loop_ss << "    " << compiled << "\n";
            }
        }
    }

    global_ss << ctx.global_stream.str();

    std::stringstream ss;
    ss << "// Generated by Dolphin transpiler for Hardware target\n";
    ss << "#include \"dolphin_hardware_runtime.hpp\"\n\n";
    ss << global_ss.str() << "\n";
    ss << "void setup() {\n";
    ss << "    Serial.begin(115200);\n";
    ss << setup_ss.str();
    ss << "}\n\n";
    ss << "void loop() {\n";
    ss << loop_ss.str();
    ss << "    DolphinRuntime::pollPins();\n";
    ss << "}\n";

    return ss.str();
}
