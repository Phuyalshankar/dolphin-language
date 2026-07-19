CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread

.PHONY: all clean run

all: transpiler dolphin

transpiler: transpiler.cpp generator.cpp parser.cpp lexer.cpp utils.cpp checker.cpp
	$(CXX) $(CXXFLAGS) transpiler.cpp generator.cpp parser.cpp lexer.cpp utils.cpp checker.cpp -o transpiler

dolphin: dolphin.cpp lexer.cpp parser.cpp generator.cpp repl.cpp utils.cpp checker.cpp
	$(CXX) $(CXXFLAGS) dolphin.cpp lexer.cpp parser.cpp generator.cpp repl.cpp utils.cpp checker.cpp -o dolphin

# Usage: make run FILE=test.dolphin
run: all
	./dolphin run $(FILE)

clean:
	rm -f transpiler dolphin *.o temp_dolphin_output.cpp temp_dolphin_output
