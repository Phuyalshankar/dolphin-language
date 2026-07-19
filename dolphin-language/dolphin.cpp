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
#include "checker.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <algorithm>

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

// ─── Binary location helper ─────────────────────────────────────────────────
// Returns the directory where the dolphin.exe binary is located
static std::string get_binary_dir(const std::string& argv0) {
    // 1. Try canonical path of argv0
    std::error_code ec;
    auto self = std::filesystem::canonical(argv0, ec);
    if (!ec && self.has_parent_path()) {
        return self.parent_path().string();
    }
    // 2. argv0 has a directory part (e.g. .\dolphin.exe or D:\path\dolphin.exe)
    std::filesystem::path p(argv0);
    if (p.has_parent_path() && !p.parent_path().empty() && p.parent_path() != ".") {
        std::error_code ec2;
        auto abs = std::filesystem::canonical(p.parent_path(), ec2);
        if (!ec2) return abs.string();
        return p.parent_path().string();
    }
    // 3. Fallback — current working directory
    return std::filesystem::current_path().string();
}

// Returns all candidate directories to search for dolphin_runtime.hpp
static std::vector<std::string> runtime_candidates(const std::string& binary_dir,
                                                    const std::string& source_dir) {
    std::vector<std::string> v;
    // DOLPHIN_HOME env var (highest priority)
    if (const char* h = std::getenv("DOLPHIN_HOME")) {
        v.push_back(h);
        v.push_back(std::string(h) + "/dolphin-language");
    }
    v.push_back(binary_dir);
    v.push_back(binary_dir + "/dolphin-language");
    v.push_back(source_dir);
    v.push_back(".");
    return v;
}

// Returns the best runtime include directory
static std::string find_runtime_dir(const std::string& binary_dir,
                                    const std::string& source_dir) {
    for (auto& d : runtime_candidates(binary_dir, source_dir)) {
        if (std::filesystem::exists(d + "/dolphin_runtime.hpp")) {
            return d;
        }
    }
    return binary_dir;  // let the compiler report the error
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

static void print_semantic_error(const SemanticError& e) {
    std::cerr << col_red("error[E003]") << ": " << col_bold(e.message) << "\n"
              << col_cyan(" --> ") << e.filepath << ":" << e.line << ":" << e.column << "\n"
              << "  |\n"
              << e.line << " |  " << e.source_line << "\n"
              << "  |  " << std::string(e.column - 1, ' ') << col_red("^") << "\n"
              << "  |\n";
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

    // Run compile-time semantic analysis and static type checking
    StaticAnalyzer analyzer(source_path);
    analyzer.analyze(ast);

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
        std::cout << col_green("  ok  ") << source_path << " — syntax and semantic checks OK\n";
        return 0;
    } catch (const SemanticError& e) {
        print_semantic_error(e);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

// ─── Command: init ───────────────────────────────────────────────────────────
// Copies runtime headers into the current working directory so `dolphin run`
// works from any folder without manual setup.
static int cmd_init(const std::string& binary_dir) {
    std::string cwd = std::filesystem::current_path().string();
    std::cout << col_bold("  init   ") << cwd << "\n";

    // Find source directory for runtime files:
    // Priority: DOLPHIN_HOME env > same dir as exe > dolphin-language/ subdir
    std::vector<std::string> search_dirs;
    if (const char* h = std::getenv("DOLPHIN_HOME")) {
        search_dirs.push_back(h);
        search_dirs.push_back(std::string(h) + "/dolphin-language");
    }
    search_dirs.push_back(binary_dir);
    search_dirs.push_back(binary_dir + "/dolphin-language");

    // Find which dir has dolphin_runtime.hpp
    std::string src_base;
    for (auto& d : search_dirs) {
        if (std::filesystem::exists(d + "/dolphin_runtime.hpp")) {
            src_base = d;
            break;
        }
    }

    if (src_base.empty()) {
        std::cerr << col_red("error: ") << "Cannot find Dolphin runtime files!\n"
                  << "Searched in:\n";
        for (auto& d : search_dirs) std::cerr << "  - " << d << "\n";
        std::cerr << "\nFix options:\n"
                  << "  1. Set DOLPHIN_HOME=C:\\path\\to\\dolphin-language\n"
                  << "  2. Keep dolphin.exe in the same folder as dolphin_runtime.hpp\n";
        return 1;
    }

    std::cout << col_cyan("  from   ") << src_base << "\n";

    // Copy top-level headers
    struct FileCopy { std::string src; std::string dst_name; };
    std::vector<FileCopy> top_files = {
        { src_base + "/dolphin_runtime.hpp",          "dolphin_runtime.hpp" },
        { src_base + "/dolphin_hardware_runtime.hpp", "dolphin_hardware_runtime.hpp" },
    };

    bool any_error = false;
    for (auto& f : top_files) {
        if (!std::filesystem::exists(f.src)) {
            std::cerr << col_red("  miss  ") << f.dst_name << "\n";
            continue;  // non-fatal, skip
        }
        std::string dst = cwd + "/" + f.dst_name;
        std::error_code ec;
        std::filesystem::copy_file(f.src, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { std::cerr << col_red("  fail  ") << f.dst_name << ": " << ec.message() << "\n"; any_error = true; }
        else      std::cout << col_green("  copy  ") << f.dst_name << "\n";
    }

    // Copy runtime/ subfolder
    std::string runtime_src = src_base + "/runtime";
    if (std::filesystem::exists(runtime_src)) {
        std::string runtime_dst = cwd + "/runtime";
        std::filesystem::create_directories(runtime_dst);
        for (auto& entry : std::filesystem::directory_iterator(runtime_src)) {
            std::string dstf = runtime_dst + "/" + entry.path().filename().string();
            std::error_code ec;
            std::filesystem::copy_file(entry.path(), dstf,
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) { std::cerr << col_red("  fail  ") << entry.path().filename().string() << ": " << ec.message() << "\n"; any_error = true; }
            else      std::cout << col_green("  copy  ") << "runtime/" << entry.path().filename().string() << "\n";
        }
    } else {
        std::cerr << col_red("  miss  ") << "runtime/ subfolder not found in " << src_base << "\n";
    }

    if (any_error) return 1;

    std::cout << col_green("\n  done  ") << "Dolphin runtime installed in: " << cwd << "\n";
    std::cout << "  You can now run:  dolphin.exe run myfile.dolphin\n";
    return 0;
}

// ─── Command: build ──────────────────────────────────────────────────────────
static int cmd_build(const std::string& source_path, const std::string& out_name,
                     const std::string& binary_dir) {
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

        // Get best runtime include dir
        std::string src_dir = src.parent_path().empty() ? "." : src.parent_path().string();
        std::string runtime_dir = find_runtime_dir(binary_dir, src_dir);

        // Compile
        int rc = compile_cpp(cpp_out, bin_out, runtime_dir);
        if (rc != 0) {
            std::cerr << col_red("error") << ": C++ compilation failed (exit " << rc << ")\n";
            return 1;
        }

        std::cout << col_green("  built  ") << bin_out << "\n";
        return 0;
    } catch (const SemanticError& e) {
        print_semantic_error(e);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << col_red("error") << ": " << e.what() << "\n";
        return 1;
    }
}

// ─── Command: run ─────────────────────────────────────────────────────────────
static int cmd_run(const std::string& source_path, const std::string& binary_dir) {
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

        // Determine best runtime include dir
        std::filesystem::path src(source_path);
        std::string src_dir = src.parent_path().empty() ? "." : src.parent_path().string();
        std::string runtime_dir = find_runtime_dir(binary_dir, src_dir);

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
    } catch (const SemanticError& e) {
        print_semantic_error(e);
        return 1;
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

        // Copy runtime header
        std::string runtime_src = "dolphin_hardware_runtime.hpp";
        if (!std::filesystem::exists(runtime_src)) {
            runtime_src = "dolphin-language/dolphin_hardware_runtime.hpp";
        }
        if (std::filesystem::exists(runtime_src)) {
            std::filesystem::remove(sketchDir + "/dolphin_hardware_runtime.hpp");
            std::filesystem::copy_file(runtime_src, sketchDir + "/dolphin_hardware_runtime.hpp", std::filesystem::copy_options::overwrite_existing);
        } else {
            std::cerr << col_red("error") << ": dolphin_hardware_runtime.hpp not found!\n";
            return 1;
        }
    } catch (const SemanticError& e) {
        std::cerr << col_red("error") << ": transpilation failed:\n";
        print_semantic_error(e);
        return 1;
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
    // Resolve binary directory once
    std::string binary_dir = get_binary_dir(argv[0]);

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

    // dolphin init — setup runtime headers in current directory
    if (cmd == "init") {
        return cmd_init(binary_dir);
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
        return cmd_build(argv[2], out_name, binary_dir);
    }

    if (cmd == "flash") {
        return cmd_flash(argc, argv);
    }

    if (cmd == "run") {
        if (argc < 3) {
            std::cerr << col_red("error") << ": 'run' requires a file argument\n";
            return 1;
        }
        return cmd_run(argv[2], binary_dir);
    }

    // If argv[1] ends in .dolphin — treat as implicit "run"
    if (cmd.size() > 8 && cmd.substr(cmd.size() - 8) == ".dolphin") {
        return cmd_run(cmd, binary_dir);
    }

    std::cerr << col_red("error") << ": unknown command '" << cmd << "'\n\n";
    print_help();
    return 1;
}
