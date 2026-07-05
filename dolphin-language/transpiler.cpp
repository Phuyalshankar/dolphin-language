#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"
#include "utils.hpp"

static Program loadFile(const std::string& filepath);

static void flattenImportsInto(Program& out, Program& source, const std::string& baseDir, std::set<std::string>& imported) {
    for (auto& stmtPtr : source) {
        if (stmtPtr->kind == StmtKind::Import) {
            const auto& imp = static_cast<const ImportStmt&>(*stmtPtr);
            std::string modulePath = imp.path;
            if (modulePath.size() < 8 || modulePath.rfind(".dolphin") != modulePath.size() - 8) {
                modulePath += ".dolphin";
            }
            std::string fullPath = baseDir + modulePath;
            if (imported.count(fullPath)) continue;
            imported.insert(fullPath);
            Program imported_program = loadFile(fullPath);
            flattenImportsInto(out, imported_program, getDirectoryPath(fullPath), imported);
        } else {
            out.push_back(std::move(stmtPtr));
        }
    }
}

static Program loadFile(const std::string& filepath) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    std::stringstream buffer;
    buffer << infile.rdbuf();

    Lexer lexer(buffer.str());
    std::vector<Token> tokens = lexer.tokenize();

    Parser parser(std::move(tokens));
    return parser.parseProgram();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.dolphin> <output.cpp>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    try {
        Program mainProgram = loadFile(inputPath);

        std::set<std::string> imported_files;
        imported_files.insert(inputPath);

        Program flattened;
        flattenImportsInto(flattened, mainProgram, getDirectoryPath(inputPath), imported_files);

        std::stringstream global_stream;
        std::stringstream main_stream;

        Codegen codegen;
        codegen.generate(flattened, global_stream, main_stream);

        std::ofstream outfile(outputPath);
        if (!outfile.is_open()) {
            std::cerr << "Error opening output file: " << outputPath << std::endl;
            return 1;
        }

        outfile << "#include \"dolphin_runtime.hpp\"\n\n";
        outfile << global_stream.str() << "\n";
        outfile << "int main() {\n";
        outfile << "    std::srand(std::time(nullptr));\n";

        std::string main_line;
        std::stringstream main_output(main_stream.str());
        while (std::getline(main_output, main_line)) {
            if (!main_line.empty()) {
                outfile << "    " << main_line << "\n";
            } else {
                outfile << "\n";
            }
        }

        outfile << "    DolphinRuntime::runEventLoop();\n";
        outfile << "    return 0;\n";
        outfile << "}\n";

        outfile.close();
        std::cout << "Transpilation complete! Output saved to: " << outputPath << std::endl;
        return 0;
    } catch (const LexError& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (const ParseError& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
