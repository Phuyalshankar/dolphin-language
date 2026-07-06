#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// Forward declaration from generator.cpp
std::string generateCode(const std::unique_ptr<BlockStmt>& ast);

// Transpile a .dolphin source file to a .cpp output file.
// Returns the generated C++ code as a string.
std::string transpile(const std::string& source_path, const std::string& out_path) {
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
    std::string cpp_code = generateCode(ast);

    // Write
    std::ofstream outfile(out_path);
    if (!outfile.is_open()) {
        throw std::runtime_error("Cannot write output file: " + out_path);
    }
    outfile << cpp_code;
    outfile.close();

    return cpp_code;
}
