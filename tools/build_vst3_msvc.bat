@echo off
rem Builds the SH-101 VST3 with MSVC via the Visual Studio Build Tools.
rem
rem CMake's "Visual Studio 17 2022" generator cannot use this instance (the
rem standalone Build Tools install is not registered in the VS Installer
rem database, so CMake reports "the instance is not known to the Visual Studio
rem Installer").  Loading vcvars64.bat and using Ninja avoids that entirely.
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo ERROR: vcvars64.bat not found - is the C++ workload installed?
    exit /b 1
)
call "%VCVARS%" >nul
if errorlevel 1 (
    echo ERROR: failed to initialise the MSVC environment
    exit /b 1
)

set "CMAKE=C:\Users\bbhal\.local\bin\cmake.exe"
set "NINJA=C:\Users\bbhal\.local\bin\ninja.exe"
set "SRC=C:\Users\bbhal\sh101"
set "BUILD=C:\Users\bbhal\sh101\build-plugin-msvc"
set "JUCE=C:/Users/bbhal/juce-src"

echo == configuring ==
"%CMAKE%" -S "%SRC%" -B "%BUILD%" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl ^
    -DSH101_BUILD_TESTS=OFF ^
    -DSH101_BUILD_TOOLS=OFF ^
    -DSH101_BUILD_VST3=ON ^
    -DJUCE_DIR="%JUCE%"
if errorlevel 1 (
    echo == configure failed ==
    exit /b 1
)

echo == building VST3 ==
"%CMAKE%" --build "%BUILD%" --target SH101Plugin_VST3 sh101_host_test
if errorlevel 1 (
    echo == build failed ==
    exit /b 1
)

echo == done ==
endlocal
exit /b 0
