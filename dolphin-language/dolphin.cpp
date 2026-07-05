#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Dolphin CLI v1.0\n";
        std::cout << "Usage:\n";
        std::cout << "  dolphin run <filename.dolphin>\n";
        return 1;
    }

    std::string command = argv[1];
    std::string filename = argv[2];

    if (command == "run") {
#ifdef _WIN32
        const std::string exe_ext = ".exe";
        const std::string transpiler_bin = "transpiler.exe";
        const std::string rm_cmd = "del";
#else
        const std::string exe_ext = "";
        const std::string transpiler_bin = "./transpiler";
        const std::string rm_cmd = "rm -f";
#endif
        const std::string temp_cpp = "temp_dolphin_output.cpp";
        const std::string temp_exe = "temp_dolphin_output" + exe_ext;

        // Step 1: Transpile
        std::string transpile_cmd = transpiler_bin + " " + filename + " " + temp_cpp;
        int r1 = std::system(transpile_cmd.c_str());
        if (r1 != 0) {
            std::cerr << "Error: Transpilation failed.\n";
            return r1;
        }

        // Step 2: Compile C++
        std::string compile_cmd = "g++ -std=c++17 " + temp_cpp + " -o " + temp_exe;
#ifdef _WIN32
        compile_cmd += " -static -lws2_32";
#else
        compile_cmd += " -pthread";
#endif
        int r2 = std::system(compile_cmd.c_str());
        if (r2 != 0) {
            std::cerr << "Error: C++ compilation failed.\n";
            return r2;
        }

        // Step 3: Run the output
        std::string run_cmd = exe_ext.empty() ? ("./" + temp_exe) : temp_exe;
        int r3 = std::system(run_cmd.c_str());

        // Step 4: Cleanup
        std::system((rm_cmd + " " + temp_cpp).c_str());
        std::system((rm_cmd + " " + temp_exe).c_str());

        return r3;
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        return 1;
    }
}
