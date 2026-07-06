/**
 * repl.cpp — Dolphin Interactive REPL
 *
 * Provides an interactive read-eval-print loop for Dolphin.
 * Architecture:
 *   - Maintains an accumulated "session" of Dolphin statements.
 *   - For each new input block the user submits, appends it to the session,
 *     transpiles the full session to C++, compiles to a shared library /
 *     static binary, runs it, and shows only the NEW output lines.
 *   - Multi-line input: a block is complete when open { } are balanced.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <set>
#include <algorithm>
#include <functional>

#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"

namespace fs = std::filesystem;

// ─── ANSI colours ─────────────────────────────────────────────────────────────
#ifdef _WIN32
    #include <windows.h>
    static bool _ansi_checked = false;
    static bool _ansi_ok      = false;
    static bool ansi_enabled() {
        if (_ansi_checked) return _ansi_ok;
        _ansi_checked = true;
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        GetConsoleMode(h, &mode);
        _ansi_ok = (SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0);
        return _ansi_ok;
    }
    #define ANSI_OK ansi_enabled()
#else
    #define ANSI_OK true
#endif

static std::string c_cyan(const std::string& s)  { return ANSI_OK ? "\033[1;36m" + s + "\033[0m" : s; }
static std::string c_red(const std::string& s)   { return ANSI_OK ? "\033[1;31m" + s + "\033[0m" : s; }
static std::string c_dim(const std::string& s)   { return ANSI_OK ? "\033[2m"    + s + "\033[0m" : s; }
static std::string c_bold(const std::string& s)  { return ANSI_OK ? "\033[1m"    + s + "\033[0m" : s; }

static constexpr const char* DOLPHIN_VERSION = "2.0.0";

// ─── Input helpers ────────────────────────────────────────────────────────────

/** Count unmatched open braces to decide if input is complete. */
static int brace_depth(const std::string& line) {
    int depth = 0;
    bool in_str = false;
    char prev = '\0';
    for (char c : line) {
        if (c == '"' && prev != '\\') in_str = !in_str;
        if (!in_str) {
            if (c == '{') depth++;
            else if (c == '}') depth--;
        }
        prev = c;
    }
    return depth;
}

/** Read one logical block from stdin (handles multi-line { }). */
static std::string read_block() {
    std::string block;
    int depth = 0;
    bool first = true;

    while (true) {
        if (first)
            std::cout << c_cyan(">>> ") << std::flush;
        else
            std::cout << c_dim("... ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF
            std::cout << "\n";
            return "";
        }

        block += line + "\n";
        depth += brace_depth(line);
        first = false;

        // Block is complete when depth returns to 0 and we have something
        if (depth <= 0 && !block.empty() && block.find_first_not_of(" \t\r\n") != std::string::npos)
            break;
    }
    return block;
}

// ─── Compilation helpers ──────────────────────────────────────────────────────

static std::string find_compiler_repl() {
#ifdef _WIN32
    auto check = [](const std::string& name) {
        return std::system(("where " + name + " >NUL 2>&1").c_str()) == 0;
    };
    if (check("g++"))     return "g++";
    if (check("clang++")) return "clang++";
    if (check("cl"))      return "cl";
#else
    auto check = [](const std::string& name) {
        return std::system(("which " + name + " >/dev/null 2>&1").c_str()) == 0;
    };
    if (check("g++"))     return "g++";
    if (check("clang++")) return "clang++";
#endif
    return "";
}

static fs::path find_runtime_dir() {
    if (fs::exists("dolphin_runtime.hpp")) return ".";
    // Try next to executable
    return ".";
}

// ─── Session state ────────────────────────────────────────────────────────────

struct ReplSession {
    std::string accumulated_source;   // all Dolphin code so far
    std::string compiler;
    fs::path    runtime_dir;
    fs::path    tmp_dir;
    int         eval_count = 0;

    ReplSession() {
        compiler    = find_compiler_repl();
        runtime_dir = find_runtime_dir();
        tmp_dir     = fs::temp_directory_path();
    }

    /**
     * Transpile `source` string → C++ source string.
     * Returns empty string + sets `err` on failure.
     */
    std::string transpile(const std::string& source, std::string& err) {
        Lexer lexer(source);
        std::vector<Token> tokens;
        try {
            tokens = lexer.tokenize();
        } catch (const std::exception& e) {
            err = std::string("[Lex error] ") + e.what();
            return "";
        }

        Parser parser(tokens, "<repl>");
        std::unique_ptr<BlockStmt> ast;
        try {
            ast = parser.parse();
        } catch (const std::exception& e) {
            err = std::string(e.what());
            return "";
        }
        if (!ast) { err = "Parse failed."; return ""; }

        CodegenContext ctx;
        ctx.scope_stack.push_back({ Scope::SCOPE_BLOCK, std::set<std::string>() });
        std::stringstream body;
        for (auto& stmt : ast->statements) {
            if (stmt) body << stmt->compile(ctx);
        }

        std::ostringstream cpp;
        cpp << "#include \"dolphin_runtime.hpp\"\nusing namespace DolphinRuntime;\n\n";
        cpp << ctx.global_stream.str() << "\n";
        cpp << "void dolphin_main() {\n    std::srand(std::time(nullptr));\n";

        std::istringstream blines(body.str());
        std::string ln;
        while (std::getline(blines, ln)) {
            if (!ln.empty()) cpp << "    " << ln << "\n";
            else             cpp << "\n";
        }
        cpp << "}\n"
               "int main() { dolphin_main(); DolphinRuntime::runEventLoop(); return 0; }\n";
        return cpp.str();
    }

    /**
     * Evaluate a snippet: append to session, transpile, compile, run.
     * Returns true on success.
     */
    bool eval(const std::string& snippet) {
        if (compiler.empty()) {
            std::cerr << c_red("Error") << ": No C++ compiler found. "
                         "Install g++ or clang++.\n";
            return false;
        }

        // Try the snippet appended to accumulated source
        std::string candidate = accumulated_source + snippet;

        std::string err;
        std::string cpp_src = transpile(candidate, err);
        if (cpp_src.empty()) {
            // Maybe it's an expression — wrap in print()
            std::string expr_try = accumulated_source
                + "print(" + strip_newline(snippet) + ")\n";
            cpp_src = transpile(expr_try, err);
            if (cpp_src.empty()) {
                std::cerr << c_red("Error: ") << err << "\n";
                return false;
            }
            candidate = expr_try;
        }

        eval_count++;
        auto cpp_path = tmp_dir / ("dolphin_repl_" + std::to_string(eval_count) + ".cpp");
        auto bin_path = tmp_dir / ("dolphin_repl_" + std::to_string(eval_count));
#ifdef _WIN32
        auto bin_exe  = tmp_dir / ("dolphin_repl_" + std::to_string(eval_count) + ".exe");
#else
        auto& bin_exe = bin_path;
#endif

        // Write .cpp
        { std::ofstream f(cpp_path); f << cpp_src; }

        // Compile (suppress output unless verbose)
        std::ostringstream cc;
#ifdef _WIN32
        cc << "\"" << compiler << "\" -std=c++17 -O0"
           << " -I\"" << runtime_dir.string() << "\""
           << " -lws2_32"
           << " \"" << cpp_path.string() << "\""
           << " -o \"" << bin_exe.string() << "\""
           << " 2>NUL";
#else
        cc << compiler << " -std=c++17 -O0"
           << " -I\"" << runtime_dir.string() << "\""
           << " -lpthread"
           << " \"" << cpp_path.string() << "\""
           << " -o \"" << bin_exe.string() << "\""
           << " 2>/dev/null";
#endif
        std::error_code ec;
        int rc = std::system(cc.str().c_str());
        fs::remove(cpp_path, ec);

        if (rc != 0) {
            // Try again with verbose output so the user sees the error
            std::ostringstream cc2;
#ifdef _WIN32
            cc2 << "\"" << compiler << "\" -std=c++17 -O0"
                << " -I\"" << runtime_dir.string() << "\""
                << " -lws2_32"
                << " \"" << cpp_path.string() << "\""
                << " -o \"" << bin_exe.string() << "\"";
#else
            cc2 << compiler << " -std=c++17 -O0"
                << " -I\"" << runtime_dir.string() << "\""
                << " -lpthread"
                << " \"" << cpp_path.string() << "\""
                << " -o \"" << bin_exe.string() << "\"";
#endif
            { std::ofstream f(cpp_path); f << cpp_src; }
            std::system(cc2.str().c_str());
            fs::remove(cpp_path, ec);
            std::cerr << c_red("[Compile error]") << " The code above caused a compiler error.\n";
            return false;
        }

        // Run
#ifdef _WIN32
        rc = std::system(("\"" + bin_exe.string() + "\"").c_str());
#else
        rc = std::system(("\"" + bin_exe.string() + "\"").c_str());
#endif
        fs::remove(bin_exe, ec);

        if (rc != 0 && rc != 256) {
            std::cerr << c_red("[Runtime error]") << " Exit code: " << rc << "\n";
        }

        // Only commit to session if successful (skip print-wrap lines)
        if (candidate == accumulated_source + snippet) {
            accumulated_source = candidate;
        }
        return true;
    }

private:
    static std::string strip_newline(const std::string& s) {
        std::string t = s;
        while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' '))
            t.pop_back();
        return t;
    }
};

// ─── REPL commands ────────────────────────────────────────────────────────────

static void print_repl_help() {
    std::cout << c_bold("Available REPL commands:\n");
    std::cout << "  .help    — show this help\n";
    std::cout << "  .clear   — clear session (forget all variables/functions)\n";
    std::cout << "  .show    — show accumulated session code\n";
    std::cout << "  .exit    — exit the REPL\n";
    std::cout << "  exit     — exit the REPL\n\n";
    std::cout << "Multi-line blocks: keep typing after '{' — submit closes when braces balance.\n";
}

// ─── Public entry point ───────────────────────────────────────────────────────

int dolphin_repl() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "\n";
    std::cout << c_cyan("🐬 Dolphin v") << c_cyan(DOLPHIN_VERSION)
              << c_cyan(" Interactive REPL") << "\n";
    std::cout << c_dim("Type '.help' for commands, '.exit' or Ctrl+C to quit.") << "\n\n";

    ReplSession session;
    if (session.compiler.empty()) {
        std::cerr << c_red("Warning") << ": No C++ compiler found — REPL will not work.\n"
                     "Install g++ or clang++ and ensure it is on PATH.\n\n";
    }

    while (true) {
        std::string block = read_block();
        if (block.empty()) break;  // EOF / Ctrl+D

        // Trim
        std::string trimmed = block;
        while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r' || trimmed.back() == ' '))
            trimmed.pop_back();

        if (trimmed.empty()) continue;

        // Meta-commands
        if (trimmed == ".exit" || trimmed == "exit" || trimmed == "quit") break;
        if (trimmed == ".help") { print_repl_help(); continue; }
        if (trimmed == ".clear") {
            session.accumulated_source.clear();
            session.eval_count = 0;
            std::cout << c_dim("Session cleared.\n");
            continue;
        }
        if (trimmed == ".show") {
            if (session.accumulated_source.empty())
                std::cout << c_dim("(empty session)\n");
            else
                std::cout << c_dim("--- session ---\n") << session.accumulated_source
                           << c_dim("--- end ---\n");
            continue;
        }

        session.eval(block);
    }

    std::cout << c_cyan("\nGoodbye! 🐬\n");
    return 0;
}
