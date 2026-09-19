@echo off
rem zig "c++" driver wrapper so CMake can use zig's bundled clang++/libc++.
rem Usage: zig-cxx.bat <compiler args...>
"C:\Users\bbhal\.local\bin\python-zig.exe" c++ %*
