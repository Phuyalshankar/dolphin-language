#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include "parser.hpp"
#include "generator.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.dolphin> <output.cpp>" << std::endl;
        return 1;
    }

    std::stringstream global_stream;
    std::stringstream main_stream;

    std::vector<Scope> scope_stack;
    scope_stack.push_back({ SCOPE_BLOCK, std::set<std::string>() }); // Global scope

    std::set<std::string> imported_files;
    imported_files.insert(argv[1]);

    transpileFile(argv[1], global_stream, main_stream, scope_stack, imported_files);

    std::ofstream outfile(argv[2]);
    if (!outfile.is_open()) {
        std::cerr << "Error opening output file: " << argv[2] << std::endl;
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
    std::cout << "Transpilation complete! Output saved to: " << argv[2] << std::endl;
    return 0;
}
