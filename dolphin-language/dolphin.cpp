#include <iostream>
#include <string>
#include <cstdlib>
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

static int flashCommand(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: dolphin flash <filename.dolphin> <board> <port>\n";
        std::cerr << "  boards: ";
        bool first = true;
        for (const auto& kv : boardFqbns()) {
            if (!first) std::cerr << ", ";
            std::cerr << kv.first;
            first = false;
        }
        std::cerr << "\n  port examples: /dev/ttyUSB0, /dev/cu.usbserial-1410, COM3\n";
        std::cerr << "\nNOTE: this must be run on YOUR local machine (with the board plugged in\n";
        std::cerr << "over USB and arduino-cli installed), not inside the Replit cloud sandbox.\n";
        return 1;
    }

    std::string filename = argv[2];
    std::string board = argv[3];
    std::string port = argv[4];

    auto it = boardFqbns().find(board);
    if (it == boardFqbns().end()) {
        std::cerr << "Error: unknown board '" << board << "'. Supported: ";
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

    if (std::system("arduino-cli version > /dev/null 2>&1") != 0) {
        std::cerr << "Error: arduino-cli not found on this machine.\n";
        std::cerr << "Install it from https://arduino.github.io/arduino-cli/latest/installation/\n";
        std::cerr << "then run: arduino-cli core install " << fqbn.substr(0, fqbn.find(':', fqbn.find(':') + 1)) << "\n";
        std::cerr << "Flashing must run locally in VS Code's terminal, not inside Replit.\n";
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

#ifdef _WIN32
    const std::string transpiler_bin = "transpiler.exe";
#else
    const std::string transpiler_bin = "./transpiler";
#endif

    std::string transpileCmd = transpiler_bin + " " + filename + " " + sketchFile + " hardware";
    std::cout << "Transpiling " << filename << " for " << board << " (" << fqbn << ")...\n";
    if (std::system(transpileCmd.c_str()) != 0) {
        std::cerr << "Error: transpilation to hardware target failed.\n";
        return 1;
    }

    std::string compileCmd = "arduino-cli compile --fqbn " + fqbn + " " + sketchDir;
    std::cout << "Compiling with arduino-cli...\n";
    if (std::system(compileCmd.c_str()) != 0) {
        std::cerr << "Error: arduino-cli compile failed. Do you have the '" << board
                   << "' board core installed? Try: arduino-cli core install " << fqbn << "\n";
        return 1;
    }

    std::string uploadCmd = "arduino-cli upload -p " + port + " --fqbn " + fqbn + " " + sketchDir;
    std::cout << "Uploading to " << port << "...\n";
    int uploadResult = std::system(uploadCmd.c_str());
    if (uploadResult != 0) {
        std::cerr << "Error: upload failed. Check that the board is connected and the port is correct.\n";
        return uploadResult;
    }

    std::cout << "Flashed successfully! Sketch kept at " << sketchDir << " for reference.\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Dolphin CLI v1.0\n";
        std::cout << "Usage:\n";
        std::cout << "  dolphin run <filename.dolphin>\n";
        std::cout << "  dolphin flash <filename.dolphin> <board> <port>\n";
        return 1;
    }

    std::string command = argv[1];
    std::string filename = argv[2];

    if (command == "flash") {
        return flashCommand(argc, argv);
    } else if (command == "run") {
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
