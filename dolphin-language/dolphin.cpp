/**
 * dolphin — cross-platform CLI for the Dolphin programming language
 *
 * Commands:
 *   dolphin run  <file.dolphin>   — transpile and execute
 *   dolphin build <file.dolphin>  — transpile only (produces a .cpp + binary)
 *   dolphin check <file.dolphin>  — syntax check (no compilation)
 *   dolphin repl                  — interactive REPL
 *   dolphin --version             — print version
 *   dolphin --help                — print help
 *
 * Environment variables:
 *   DOLPHIN_CXX    — C++ compiler to use (default: g++, clang++, or cl)
 *   DOLPHIN_FLAGS  — extra flags passed to the C++ compiler
 *   DOLPHIN_VERBOSE — set to '1' to print compiler invocations
 *
 * License: MIT — see LICENSE
 */

#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>

// Forward declarations from generator.cpp
std::string generateCode(const std::unique_ptr<BlockStmt>& ast);
std::string generateHardwareCode(const std::unique_ptr<BlockStmt>& ast);
// Forward declaration from repl.cpp (if available)
int dolphin_repl();

// ─── ANSI helpers ─────────────────────────────────────────────────────────────
// Windows headers must come before any other platform includes
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  define USE_COLOR 0
#else
#  define USE_COLOR 1
#endif

static std::string col_red(const std::string& s) {
#if USE_COLOR
    return "\033[1;31m" + s + "\033[0m";
#else
    return s;
#endif
}
static std::string col_green(const std::string& s) {
#if USE_COLOR
    return "\033[1;32m" + s + "\033[0m";
#else
    return s;
#endif
}
static std::string col_bold(const std::string& s) {
#if USE_COLOR
    return "\033[1m" + s + "\033[0m";
#else
    return s;
#endif
}
static std::string col_cyan(const std::string& s) {
#if USE_COLOR
    return "\033[1;36m" + s + "\033[0m";
#else
    return s;
#endif
}

// ─── Version info ─────────────────────────────────────────────────────────────
static const char* DOLPHIN_VERSION = "2.0.0";

static void print_version() {
    std::cout << col_bold("Dolphin") << " " << col_cyan(DOLPHIN_VERSION) << "\n"
              << "C++ transpiler-based language for IoT and embedded systems\n"
              << "https://github.com/Phuyalshankar/dolphin-language\n";
}

// ─── Help text ────────────────────────────────────────────────────────────────
static void print_help() {
    std::cout
        << col_bold("USAGE:\n")
        << "    dolphin <command> [options] [file]\n\n"
        << col_bold("COMMANDS:\n")
        << "    " << col_cyan("run")   << "   <file.dolphin>            Transpile and execute immediately\n"
        << "    " << col_cyan("build") << " <file.dolphin>            Transpile to C++ and compile to binary\n"
        << "    " << col_cyan("check") << " <file.dolphin>            Syntax-check only (no compilation)\n"
        << "    " << col_cyan("repl")  << "                           Start interactive REPL\n"
        << "    " << col_cyan("flash") << " <file.dolphin> <board> <p> Flashing compiled sketch to microcontroller\n\n"
        << col_bold("OPTIONS:\n")
        << "    -o <output>    Output binary name (for build)\n"
        << "    --help         Show this help message\n"
        << "    --version      Show version information\n\n"
        << col_bold("ENVIRONMENT VARIABLES:\n")
        << "    DOLPHIN_CXX     Override C++ compiler (default: auto-detect)\n"
        << "    DOLPHIN_FLAGS   Extra compiler flags\n"
        << "    DOLPHIN_VERBOSE Set to '1' for verbose compiler output\n\n"
        << col_bold("EXAMPLES:\n")
        << "    dolphin run hello.dolphin\n"
        << "    dolphin build main.dolphin -o myapp\n"
        << "    dolphin check mylib.dolphin\n"
        << "    dolphin repl\n"
        << "    dolphin flash blink.dolphin esp32 COM3\n";
}

// ─── Compiler detection ───────────────────────────────────────────────────────
static std::string detect_compiler() {
    if (const char* env = std::getenv("DOLPHIN_CXX")) return env;

    // Try compilers in preference order
    const char* candidates[] = { "g++", "clang++", "c++", "cl" };
    for (auto& c : candidates) {
#ifdef _WIN32
        std::string check = std::string("where ") + c + " >NUL 2>&1";
#else
        std::string check = std::string("command -v ") + c + " >/dev/null 2>&1";
#endif
        if (std::system(check.c_str()) == 0) return c;
    }
    return "g++";  // fallback
}

// ─── Temp directory ───────────────────────────────────────────────────────────
static std::string make_temp_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetTempPath(MAX_PATH, buf);
    std::string base = std::string(buf) + "dolphin_";
#else
    std::string base = "/tmp/dolphin_";
#endif
    std::string dir = base + std::to_string(std::rand());
    std::filesystem::create_directories(dir);
    return dir;
}

// ─── Transpile source → C++ string ───────────────────────────────────────────
static std::string transpile_source(const std::string& source_path, bool hardware_target = false) {
    std::ifstream infile(source_path);
    if (!infile.is_open())
        throw std::runtime_error("Cannot open file: " + source_path);
    std::stringstream buf;
    buf << infile.rdbuf();
    std::string source = buf.str();

    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, source_path);
    auto ast = parser.parse();
    return hardware_target ? generateHardwareCode(ast) : generateCode(ast);
}

// ─── Write C++ file ───────────────────────────────────────────────────────────
static void write_cpp(const std::string& path, const std::string& code) {
    std::ofstream out(path);
    if (!out.is_open()) throw std::runtime_error("Cannot write: " + path);
    out << code;
}

// ─── Compile C++ → binary ─────────────────────────────────────────────────────
static int compile_cpp(const std::string& cpp_path, const std::string& out_path,
                        const std::string& runtime_include_dir) {
    std::string cxx       = detect_compiler();
    std::string extra     = "";
    if (const char* f = std::getenv("DOLPHIN_FLAGS")) extra = f;
    bool verbose          = (std::getenv("DOLPHIN_VERBOSE") != nullptr);

    // Find the runtime header — look relative to the binary's location
    std::string include_flag = "-I" + runtime_include_dir;

    // C++17 required
    std::string cmd = cxx + " -std=c++17 -O2 " +
                      include_flag + " " +
                      extra + " " +
                      cpp_path + " -o " + out_path;

#ifdef _WIN32
    cmd += " -static -lws2_32";
#else
    cmd += " 2>&1";  // merge stderr for display
#endif

    if (verbose) {
        std::cout << col_cyan("  compile: ") << cmd << "\n";
    }

    return std::system(cmd.c_str());
}

// ─── Command: check ──────────────────────────────────────────────────────────
static int cmd_check(const std::string& source_path) {
    try {
        transpile_source(source_path);
        std::cout << col_green("  ok  ") << source_path << " — syntax OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

// ─── Command: build ──────────────────────────────────────────────────────────
static int cmd_build(const std::string& source_path, const std::string& out_name) {
    try {
        std::cout << col_bold("  build  ") << source_path << "\n";

        // Determine output paths
        std::filesystem::path src(source_path);
        std::string base_name  = src.stem().string();
        std::string cpp_out    = base_name + ".cpp";
        std::string bin_out    = out_name.empty() ? base_name : out_name;

#ifdef _WIN32
        if (bin_out.rfind(".exe") == std::string::npos) bin_out += ".exe";
#endif

        // Transpile
        std::string cpp_code = transpile_source(source_path);
        write_cpp(cpp_out, cpp_code);
        std::cout << col_cyan("  write  ") << cpp_out << "\n";

        // Get runtime include dir — same directory as this source or binary
        std::string runtime_dir = src.parent_path().empty() ? "." : src.parent_path().string();

        // Compile
        int rc = compile_cpp(cpp_out, bin_out, runtime_dir);
        if (rc != 0) {
            std::cerr << col_red("error") << ": C++ compilation failed (exit " << rc << ")\n";
            return 1;
        }

        std::cout << col_green("  built  ") << bin_out << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << col_red("error") << ": " << e.what() << "\n";
        return 1;
    }
}

// ─── Command: run ─────────────────────────────────────────────────────────────
static int cmd_run(const std::string& source_path) {
    try {
        std::cout << col_bold("  run   ") << source_path << "\n";

        // Transpile to a temp directory
        std::string tmp_dir  = make_temp_dir();
        std::string cpp_out  = tmp_dir + "/program.cpp";
        std::string bin_out  = tmp_dir + "/program";
#ifdef _WIN32
        bin_out += ".exe";
#endif

        std::string cpp_code = transpile_source(source_path);
        write_cpp(cpp_out, cpp_code);

        // Determine runtime include dir — the directory of the source file
        std::filesystem::path src(source_path);
        std::string runtime_dir = src.parent_path().empty() ? "." : src.parent_path().string();

        int rc = compile_cpp(cpp_out, bin_out, runtime_dir);
        if (rc != 0) {
            std::cerr << col_red("error") << ": C++ compilation failed (exit " << rc << ")\n";
            // Clean up
            std::filesystem::remove_all(tmp_dir);
            return 1;
        }

        // Execute
        int result = std::system(bin_out.c_str());

        // Clean up temp dir
        std::filesystem::remove_all(tmp_dir);
        return result;
    } catch (const std::exception& e) {
        std::cerr << col_red("error") << ": " << e.what() << "\n";
        return 1;
    }
}

#include <map>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static const std::map<std::string, std::string>& boardFqbns() {
    static const std::map<std::string, std::string> table = {
        {"esp32", "esp32:esp32:esp32"},
        {"esp8266", "esp8266:esp8266:nodemcuv2"},
        {"uno", "arduino:avr:uno"},
        {"nano", "arduino:avr:nano"},
        {"mega", "arduino:avr:mega"},
        {"stm32", "STMicroelectronics:stm32:GenF1"},
    };
    return table;
}

static std::string baseName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

static int cmd_flash(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << col_red("error") << ": 'flash' requires file, board, and port arguments\n";
        std::cerr << "Usage: dolphin flash <filename.dolphin> <board> <port>\n";
        std::cerr << "  boards: ";
        bool first = true;
        for (const auto& kv : boardFqbns()) {
            if (!first) std::cerr << ", ";
            std::cerr << kv.first;
            first = false;
        }
        std::cerr << "\n  port examples: /dev/ttyUSB0, COM3\n";
        return 1;
    }

    std::string filename = argv[2];
    std::string board = argv[3];
    std::string port = argv[4];

    auto it = boardFqbns().find(board);
    if (it == boardFqbns().end()) {
        std::cerr << col_red("error") << ": unknown board '" << board << "'. Supported: ";
        bool first = true;
        for (const auto& kv : boardFqbns()) {
            if (!first) std::cerr << ", ";
            std::cerr << kv.first;
            first = false;
        }
        std::cerr << "\n";
        return 1;
    }
    std::string fqbn = it->second;

#ifdef _WIN32
    std::string checkCmd = "arduino-cli version > NUL 2>&1";
#else
    std::string checkCmd = "arduino-cli version > /dev/null 2>&1";
#endif

    if (std::system(checkCmd.c_str()) != 0) {
        std::cerr << col_red("error") << ": arduino-cli not found on this machine.\n";
        std::cerr << "Install it from https://arduino.github.io/arduino-cli/latest/installation/\n";
        std::cerr << "then run: arduino-cli core install " << fqbn.substr(0, fqbn.find(':', fqbn.find(':') + 1)) << "\n";
        return 1;
    }

    std::string name = baseName(filename);
    std::string sketchDir = name + "_sketch";
    std::string sketchFile = sketchDir + "/" + name + "_sketch.ino";

#ifdef _WIN32
    _mkdir(sketchDir.c_str());
#else
    mkdir(sketchDir.c_str(), 0755);
#endif

    std::cout << "Transpiling " << filename << " for " << board << " (" << fqbn << ")...\n";
    try {
        std::string cpp_code = transpile_source(filename, true);
        
        std::ofstream outfile(sketchFile);
        if (!outfile.is_open()) {
            std::cerr << col_red("error") << ": Cannot write sketch file: " << sketchFile << "\n";
            return 1;
        }
        outfile << cpp_code;
        outfile.close();
    } catch (const std::exception& e) {
        std::cerr << col_red("error") << ": transpilation failed: " << e.what() << "\n";
        return 1;
    }

    std::string compileCmd = "arduino-cli compile --fqbn " + fqbn + " " + sketchDir;
    std::cout << "Compiling with arduino-cli...\n";
    if (std::system(compileCmd.c_str()) != 0) {
        std::cerr << col_red("error") << ": arduino-cli compile failed.\n";
        return 1;
    }

    std::string uploadCmd = "arduino-cli upload -p " + port + " --fqbn " + fqbn + " " + sketchDir;
    std::cout << "Uploading to " << port << "...\n";
    int uploadResult = std::system(uploadCmd.c_str());
    if (uploadResult != 0) {
        std::cerr << col_red("error") << ": upload failed. Check port and connections.\n";
        return uploadResult;
    }

    std::cout << col_green("  success  ") << "Flashed successfully! Sketch kept at " << sketchDir << ".\n";
    return 0;
}

// ─── Entry point ─────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "--version" || cmd == "-v" || cmd == "version") {
        print_version();
        return 0;
    }

    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        print_help();
        return 0;
    }

    if (cmd == "repl") {
        return dolphin_repl();
    }

    if (cmd == "check") {
        if (argc < 3) {
            std::cerr << col_red("error") << ": 'check' requires a file argument\n";
            return 1;
        }
        return cmd_check(argv[2]);
    }

    if (cmd == "build") {
        if (argc < 3) {
            std::cerr << col_red("error") << ": 'build' requires a file argument\n";
            return 1;
        }
        std::string out_name;
        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i+1 < argc) {
                out_name = argv[++i];
            }
        }
        return cmd_build(argv[2], out_name);
    }

    if (cmd == "flash") {
        return cmd_flash(argc, argv);
    }

    if (cmd == "run") {
        if (argc < 3) {
            std::cerr << col_red("error") << ": 'run' requires a file argument\n";
            return 1;
        }
        return cmd_run(argv[2]);
    }

    // If argv[1] ends in .dolphin — treat as implicit "run"
    if (cmd.size() > 8 && cmd.substr(cmd.size() - 8) == ".dolphin") {
        return cmd_run(cmd);
    }

    std::cerr << col_red("error") << ": unknown command '" << cmd << "'\n\n";
    print_help();
    return 1;
}
