@echo off
rem zig "cc" driver wrapper so CMake/tools that expect a compiler executable can
rem use zig's bundled clang. Usage: zig-cc.bat <compiler args...>
"C:\Users\bbhal\.local\bin\python-zig.exe" cc %*
