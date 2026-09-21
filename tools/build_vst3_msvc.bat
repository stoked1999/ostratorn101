@echo off
rem Builds the ÖstraTorn101 VST3 with MSVC (Windows) + Ninja.
rem
rem Why Ninja and not the "Visual Studio 17 2022" CMake generator: that generator
rem needs the build-tools instance registered in the VS Installer database, which
rem a standalone Build Tools install is not — CMake then reports "the instance is
rem not known to the Visual Studio Installer".  Loading vcvars64.bat and driving
rem Ninja sidesteps it entirely.
rem
rem Nothing in this file has to be edited to build on another machine.  Every
rem path is taken from the environment when set, and discovered when not:
rem
rem   VCVARS    vcvars64.bat to load              (the usual VS 2019/2022 places)
rem   CMAKE     cmake executable                  (PATH, then the usual places)
rem   NINJA     ninja executable                  (PATH, then the usual places)
rem   SRC       repository root                   (this script's parent directory)
rem   BUILD     build tree                        (<root>\build-plugin-msvc)
rem   JUCE_DIR  JUCE 8.x checkout                 (JUCE_DIR, then the usual places)
setlocal

rem --------------------------------------------------------------- repository --
if not defined SRC set "SRC=%~dp0.."
for %%I in ("%SRC%") do set "SRC=%%~fI"
if not exist "%SRC%\CMakeLists.txt" (
    echo ERROR: no CMakeLists.txt in "%SRC%" - set SRC to the repository root
    exit /b 1
)
if not defined BUILD set "BUILD=%SRC%\build-plugin-msvc"

rem --------------------------------------------------------------------- MSVC --
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS call :try_file VCVARS "%ProgramFiles%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS (
    echo ERROR: vcvars64.bat not found.
    echo        Install the "Desktop development with C++" workload in Visual Studio
    echo        Build Tools 2019/2022, or set VCVARS to the full path of vcvars64.bat.
    exit /b 1
)
call "%VCVARS%" >nul
if errorlevel 1 (
    echo ERROR: failed to initialise the MSVC environment from "%VCVARS%"
    exit /b 1
)

rem ------------------------------------------------------------- cmake / ninja --
for /f "delims=" %%P in ('where cmake 2^>nul') do call :try_file CMAKE "%%P"
if not defined CMAKE call :try_file CMAKE "%USERPROFILE%\.local\bin\cmake.exe"
if not defined CMAKE call :try_file CMAKE "%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE call :try_file CMAKE "%ProgramFiles(x86)%\CMake\bin\cmake.exe"
if not defined CMAKE call :try_file CMAKE "%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe"
if not defined CMAKE call :try_file CMAKE "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined CMAKE call :try_file CMAKE "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not defined CMAKE (
    echo ERROR: cmake not found.
    echo        Put cmake on PATH, or install it with:  uv tool install cmake
    exit /b 1
)

for /f "delims=" %%P in ('where ninja 2^>nul') do call :try_file NINJA "%%P"
if not defined NINJA call :try_file NINJA "%USERPROFILE%\.local\bin\ninja.exe"
if not defined NINJA call :try_file NINJA "%ProgramFiles%\CMake\bin\ninja.exe"
if not defined NINJA call :try_file NINJA "%LOCALAPPDATA%\Programs\CMake\bin\ninja.exe"
if not defined NINJA call :try_file NINJA "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not defined NINJA call :try_file NINJA "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not defined NINJA (
    echo ERROR: ninja not found.
    echo        Put ninja on PATH, or install it with:  uv tool install ninja
    exit /b 1
)

rem --------------------------------------------------------------------- JUCE --
if not defined JUCE_DIR call :try_juce JUCE_DIR "%USERPROFILE%\juce-src"
if not defined JUCE_DIR call :try_juce JUCE_DIR "%USERPROFILE%\JUCE"
if not defined JUCE_DIR call :try_juce JUCE_DIR "C:\JUCE"
if not defined JUCE_DIR call :try_juce JUCE_DIR "C:\src\JUCE"
if not defined JUCE_DIR call :try_juce JUCE_DIR "%USERPROFILE%\Documents\JUCE"
if not defined JUCE_DIR (
    echo ERROR: no JUCE checkout found.
    echo        JUCE 8.x is needed for the plugin ^(not for the DSP engine^).  Clone it:
    echo            git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git C:\JUCE
    echo        then set JUCE_DIR, or pass -DJUCE_DIR yourself.
    exit /b 1
)

set "JUCE=%JUCE_DIR:\=/%"
echo == configuring ==
echo    cmake      %CMAKE%
echo    ninja      %NINJA%
echo    vcvars     %VCVARS%
echo    source     %SRC%
echo    build      %BUILD%
echo    JUCE       %JUCE%
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
"%CMAKE%" --build "%BUILD%" --target SH101Plugin_VST3 sh101_host_test sh101_editor_test
if errorlevel 1 (
    echo == build failed ==
    exit /b 1
)

echo == done ==
endlocal
exit /b 0

rem ---------------------------------------------------------------- helpers --
rem :try_file <variable> <candidate file>  - first existing candidate wins
:try_file
if defined %1 exit /b 0
if not exist "%~2" exit /b 0
set "%1=%~2"
exit /b 0

rem :try_juce <variable> <candidate directory>  - must look like a JUCE checkout
:try_juce
if defined %1 exit /b 0
if not exist "%~2\modules\juce_core" exit /b 0
set "%1=%~2"
exit /b 0
