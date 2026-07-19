#include "checker.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

StaticAnalyzer::StaticAnalyzer(const std::string& fp) : filepath(fp) {
    std::ifstream file(fp);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            source_lines.push_back(line);
        }
    }
}

std::string StaticAnalyzer::getSourceLine(int line_num) const {
    if (line_num > 0 && line_num <= (int)source_lines.size()) {
        return source_lines[line_num - 1];
    }
    return "";
}

void StaticAnalyzer::raiseError(const std::string& msg, const ASTNode* node) const {
    int line = node ? node->line : 1;
    int col = node ? node->column : 1;
    std::string src_line = getSourceLine(line);
    throw SemanticError(msg, line, col, filepath, src_line);
}

void StaticAnalyzer::raiseError(const std::string& msg, int line, int col) const {
    std::string src_line = getSourceLine(line);
    throw SemanticError(msg, line, col, filepath, src_line);
}

void StaticAnalyzer::pushScope() {
    scope_stack.push_back({});
}

void StaticAnalyzer::popScope() {
    if (!scope_stack.empty()) {
        scope_stack.pop_back();
    }
}

void StaticAnalyzer::declareVariable(const std::string& name, const std::string& type, bool is_const, int line, int col) {
    if (!scope_stack.empty()) {
        auto& current_scope = scope_stack.back();
        if (current_scope.count(name)) {
            // Already declared in the same block scope
            std::string msg = "Redeclaration of variable '" + name + "' in the same scope. Previous declaration was at line " 
                            + std::to_string(current_scope[name].line) + ".";
            raiseError(msg, line, col);
        }
        current_scope[name] = VariableInfo{name, type, is_const, line, col};
    }
}

bool StaticAnalyzer::isDeclared(const std::string& name) const {
    // Pre-defined globals and built-ins
    static const std::vector<std::string> builtins = {
        "print", "println", "input", "len", "range", "type", "parseInt", "parseFloat",
        "str", "int", "float", "isOdd", "isEven", "setTimeout", "setInterval", "sleep",
        "Math", "JSON", "File", "Date", "Random", "Process", "Dolphin", "Wifi", "WiFi",
        "Bluetooth", "Zigbee", "CAN", "Modbus", "GPIO", "Pin", "HIGH", "LOW",
        "INPUT", "OUTPUT", "PIN_INPUT", "PIN_OUTPUT", "self", "__super__"
    };
    
    if (std::find(builtins.begin(), builtins.end(), name) != builtins.end()) {
        return true;
    }

    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
        if (it->count(name)) return true;
    }
    return false;
}

const VariableInfo* StaticAnalyzer::getVariable(const std::string& name) const {
    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &(found->second);
        }
    }
    return nullptr;
}

void StaticAnalyzer::analyze(const std::unique_ptr<BlockStmt>& ast) {
    // Clear and initialize global scope
    scope_stack.clear();
    pushScope(); // Global scope
    
    if (ast) {
        for (const auto& stmt : ast->statements) {
            checkNode(stmt.get());
        }
    }
    
    popScope();
}

void StaticAnalyzer::checkNode(const ASTNode* node) {
    if (!node) return;
    
    // Check if node is Expr or Stmt
    if (auto* expr = dynamic_cast<const Expr*>(node)) {
        checkExpr(expr);
    } else if (auto* stmt = dynamic_cast<const Stmt*>(node)) {
        checkStmt(stmt);
    }
}

void StaticAnalyzer::checkExpr(const Expr* expr) {
    if (!expr) return;

    if (auto* lit = dynamic_cast<const LiteralExpr*>(expr)) {
        // Nothing to check for raw literals
    }
    else if (auto* id = dynamic_cast<const IdentifierExpr*>(expr)) {
        if (!isDeclared(id->name)) {
            raiseError("Undeclared variable '" + id->name + "'.", id);
        }
    }
    else if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        if (bin->op == ".") {
            // Property access. E.g. obj.prop
            checkExpr(bin->lhs.get());
            // Do NOT check rhs if it's a simple identifier since it represents a property name
            if (!dynamic_cast<const IdentifierExpr*>(bin->rhs.get())) {
                checkExpr(bin->rhs.get());
            }
        } else {
            checkExpr(bin->lhs.get());
            checkExpr(bin->rhs.get());
        }
    }
    else if (auto* unary = dynamic_cast<const UnaryExpr*>(expr)) {
        if (unary->op == "++" || unary->op == "--") {
            // Modifying operator!
            if (auto* id = dynamic_cast<const IdentifierExpr*>(unary->rhs.get())) {
                if (!isDeclared(id->name)) {
                    raiseError("Undeclared variable '" + id->name + "' cannot be modified.", id);
                }
                const auto* var_info = getVariable(id->name);
                if (var_info && var_info->is_const) {
                    raiseError("Cannot modify const variable '" + id->name + "'.", id);
                }
            } else {
                checkExpr(unary->rhs.get());
            }
        } else {
            checkExpr(unary->rhs.get());
        }
    }
    else if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        checkExpr(call->callee.get());
        for (const auto& arg : call->args) {
            checkExpr(arg.get());
        }
    }
    else if (auto* idx = dynamic_cast<const IndexExpr*>(expr)) {
        checkExpr(idx->expr.get());
        checkExpr(idx->index.get());
    }
    else if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(expr)) {
        for (const auto& elem : arr->elements) {
            checkExpr(elem.get());
        }
    }
    else if (auto* obj = dynamic_cast<const ObjectLiteralExpr*>(expr)) {
        for (const auto& prop : obj->properties) {
            checkExpr(prop.second.get());
        }
    }
    else if (auto* lambda = dynamic_cast<const LambdaExpr*>(expr)) {
        pushScope();
        for (const auto& arg : lambda->args) {
            declareVariable(arg, "var", false, lambda->line, lambda->column);
        }
        checkNode(lambda->body.get());
        popScope();
    }
    else if (auto* aw = dynamic_cast<const AwaitExpr*>(expr)) {
        checkExpr(aw->expr.get());
    }
    else if (auto* ty = dynamic_cast<const TypeofExpr*>(expr)) {
        checkExpr(ty->expr.get());
    }
    else if (auto* nw = dynamic_cast<const NewExpr*>(expr)) {
        for (const auto& arg : nw->args) {
            checkExpr(arg.get());
        }
    }
    else if (auto* match_expr = dynamic_cast<const MatchExpr*>(expr)) {
        checkExpr(match_expr->val.get());
        for (const auto& arm : match_expr->arms) {
            checkExpr(arm.first.get());
            checkExpr(arm.second.get());
        }
        if (match_expr->default_arm) {
            checkExpr(match_expr->default_arm.get());
        }
    }
    else if (auto* comp = dynamic_cast<const ListComprehensionExpr*>(expr)) {
        checkExpr(comp->container.get());
        pushScope();
        declareVariable(comp->var_name, "var", false, comp->line, comp->column);
        if (comp->condition) {
            checkExpr(comp->condition.get());
        }
        checkExpr(comp->expr.get());
        popScope();
    }
}

void StaticAnalyzer::checkStmt(const Stmt* stmt) {
    if (!stmt) return;

    if (auto* block_stmt = dynamic_cast<const BlockStmt*>(stmt)) {
        pushScope();
        for (const auto& s : block_stmt->statements) {
            checkNode(s.get());
        }
        popScope();
    }
    else if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(stmt)) {
        checkExpr(expr_stmt->expr.get());
    }
    else if (auto* var_decl = dynamic_cast<const VarDeclStmt*>(stmt)) {
        if (var_decl->initializer) {
            checkExpr(var_decl->initializer.get());
        }
        declareVariable(var_decl->name, "var", false, var_decl->line, var_decl->column);
    }
    else if (auto* typed_decl = dynamic_cast<const TypedDeclStmt*>(stmt)) {
        if (typed_decl->initializer) {
            checkExpr(typed_decl->initializer.get());
        }
        
        bool is_c = (typed_decl->type_name == "const");
        
        // Type validation
        if (typed_decl->type_name == "pin" && typed_decl->initializer) {
            // Must be initialized with a Pin/pin constructor/NewExpr
            bool valid_pin = false;
            if (auto* call = dynamic_cast<const CallExpr*>(typed_decl->initializer.get())) {
                if (auto* callee_id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
                    if (callee_id->name == "pin" || callee_id->name == "Pin") {
                        valid_pin = true;
                    }
                }
            } else if (auto* nw = dynamic_cast<const NewExpr*>(typed_decl->initializer.get())) {
                if (nw->class_name == "Pin" || nw->class_name == "pin") {
                    valid_pin = true;
                }
            }
            if (!valid_pin) {
                raiseError("Type mismatch: 'pin' variable must be initialized with a pin() or new Pin() expression.", typed_decl);
            }
        }
        else if (typed_decl->initializer) {
            // Primitive literal checks
            if (auto* lit = dynamic_cast<const LiteralExpr*>(typed_decl->initializer.get())) {
                if (typed_decl->type_name == "int" && lit->kind != LiteralExpr::LIT_INT) {
                    raiseError("Type mismatch: Cannot assign non-integer literal to int variable '" + typed_decl->name + "'.", typed_decl);
                }
                if (typed_decl->type_name == "string" && lit->kind != LiteralExpr::LIT_STRING && lit->kind != LiteralExpr::LIT_TEMPLATE) {
                    raiseError("Type mismatch: Cannot assign non-string literal to string variable '" + typed_decl->name + "'.", typed_decl);
                }
                if (typed_decl->type_name == "bool" && lit->kind != LiteralExpr::LIT_BOOL) {
                    raiseError("Type mismatch: Cannot assign non-boolean literal to bool variable '" + typed_decl->name + "'.", typed_decl);
                }
            }
        }

        declareVariable(typed_decl->name, typed_decl->type_name, is_c, typed_decl->line, typed_decl->column);
    }
    else if (auto* assign = dynamic_cast<const AssignStmt*>(stmt)) {
        checkExpr(assign->rhs.get());
        
        if (auto* id = dynamic_cast<const IdentifierExpr*>(assign->lhs.get())) {
            if (isDeclared(id->name)) {
                // If it is declared, check if it's constant
                const auto* var_info = getVariable(id->name);
                if (var_info && var_info->is_const) {
                    raiseError("Cannot reassign const variable '" + id->name + "'.", assign);
                }
            } else {
                // First-time implicit assignment
                if (assign->op == "=") {
                    // Declare it implicitly in the current scope as a var
                    declareVariable(id->name, "var", false, id->line, id->column);
                } else {
                    // Compound assignment on undeclared variable
                    raiseError("Undeclared variable '" + id->name + "' cannot be modified.", assign);
                }
            }
        } else {
            checkExpr(assign->lhs.get());
        }
    }
    else if (auto* fn_decl = dynamic_cast<const FuncDeclStmt*>(stmt)) {
        declareVariable(fn_decl->name, "function", false, fn_decl->line, fn_decl->column);
        
        pushScope();
        for (const auto& arg : fn_decl->args) {
            declareVariable(arg, "var", false, fn_decl->line, fn_decl->column);
        }
        checkNode(fn_decl->body.get());
        popScope();
    }
    else if (auto* if_stmt = dynamic_cast<const IfStmt*>(stmt)) {
        checkExpr(if_stmt->condition.get());
        checkNode(if_stmt->then_branch.get());
        if (if_stmt->else_branch) {
            checkNode(if_stmt->else_branch.get());
        }
    }
    else if (auto* while_stmt = dynamic_cast<const WhileStmt*>(stmt)) {
        checkExpr(while_stmt->condition.get());
        checkNode(while_stmt->body.get());
    }
    else if (auto* for_range = dynamic_cast<const ForRangeStmt*>(stmt)) {
        checkExpr(for_range->start.get());
        checkExpr(for_range->end.get());
        
        pushScope();
        declareVariable(for_range->var_name, "int", false, for_range->line, for_range->column);
        checkNode(for_range->body.get());
        popScope();
    }
    else if (auto* for_each = dynamic_cast<const ForEachStmt*>(stmt)) {
        checkExpr(for_each->container.get());
        
        pushScope();
        declareVariable(for_each->var_name, "var", false, for_each->line, for_each->column);
        checkNode(for_each->body.get());
        popScope();
    }
    else if (auto* inf_loop = dynamic_cast<const InfiniteLoopStmt*>(stmt)) {
        checkNode(inf_loop->body.get());
    }
    else if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt)) {
        if (ret->value) {
            checkExpr(ret->value.get());
        }
    }
    else if (auto* br = dynamic_cast<const BreakStmt*>(stmt)) {
        // Nothing to check
    }
    else if (auto* cont = dynamic_cast<const ContinueStmt*>(stmt)) {
        // Nothing to check
    }
    else if (auto* import_stmt = dynamic_cast<const ImportStmt*>(stmt)) {
        for (const auto& s : import_stmt->resolved_stmts) {
            checkNode(s.get());
        }
    }
    else if (auto* try_catch = dynamic_cast<const TryCatchStmt*>(stmt)) {
        checkNode(try_catch->try_block.get());
        if (try_catch->catch_block) {
            pushScope();
            declareVariable(try_catch->catch_var, "var", false, try_catch->line, try_catch->column);
            checkNode(try_catch->catch_block.get());
            popScope();
        }
        if (try_catch->finally_block) {
            checkNode(try_catch->finally_block.get());
        }
    }
    else if (auto* thr = dynamic_cast<const ThrowStmt*>(stmt)) {
        checkExpr(thr->expr.get());
    }
    else if (auto* cls_decl = dynamic_cast<const ClassDeclStmt*>(stmt)) {
        declareVariable(cls_decl->name, "class", false, cls_decl->line, cls_decl->column);
        
        for (const auto& m : cls_decl->methods) {
            pushScope();
            declareVariable("self", "var", false, cls_decl->line, cls_decl->column);
            for (const auto& p : m.params) {
                declareVariable(p, "var", false, cls_decl->line, cls_decl->column);
            }
            checkNode(m.body.get());
            popScope();
        }
    }
}
