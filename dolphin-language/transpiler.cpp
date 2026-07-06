#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// Forward declarations from generator.cpp
std::string generateCode(const std::unique_ptr<BlockStmt>& ast);
std::string generateHardwareCode(const std::unique_ptr<BlockStmt>& ast);

// Transpile a .dolphin source file to a .cpp output file.
// Returns the generated C++ code as a string.
std::string transpile(const std::string& source_path, const std::string& out_path, bool hardware_target = false) {
    // Read source
    std::ifstream infile(source_path);
    if (!infile.is_open()) {
        throw std::runtime_error("Cannot open source file: " + source_path);
    }
    std::stringstream buf;
    buf << infile.rdbuf();
    std::string source = buf.str();

    // Lex
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    // Parse
    Parser parser(tokens, source_path);
    std::unique_ptr<BlockStmt> ast = parser.parse();

    // Codegen
    std::string cpp_code = hardware_target ? generateHardwareCode(ast) : generateCode(ast);

    // Write
    std::ofstream outfile(out_path);
    if (!outfile.is_open()) {
        throw std::runtime_error("Cannot write output file: " + out_path);
    }
    outfile << cpp_code;
    outfile.close();

    return cpp_code;
}

#ifdef BUILD_TRANSPILER_EXE
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: transpiler <input.dolphin> <output.cpp> [hardware]\n";
        return 1;
    }
    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    bool hardwareTarget = (argc >= 4 && std::string(argv[3]) == "hardware");
    try {
        transpile(inputPath, outputPath, hardwareTarget);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
#endif
