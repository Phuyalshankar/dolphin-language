@echo off
echo Building Dolphin Compiler...
g++ -std=c++17 -O2 dolphin.cpp lexer.cpp parser.cpp generator.cpp repl.cpp utils.cpp checker.cpp -static -lws2_32 -o dolphin.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✓ Build successful! dolphin.exe is ready
    dolphin.exe --version
) else (
    echo.
    echo ✗ Build failed!
    exit /b 1
)
