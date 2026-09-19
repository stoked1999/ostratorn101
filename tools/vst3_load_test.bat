@echo off
rem Builds and runs the VST3 load test against the installed bundle.
rem The VST3 SDK's FUID / interface-IID definitions live in SDK .cpp files, so
rem those are compiled in as well.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul

set "SRC=C:\Users\bbhal\sh101\tools\vst3_load_test.cpp"
set "SDK=C:\Users\bbhal\juce-src\modules\juce_audio_processors_headless\format_types\VST3_SDK"
set "OUT=C:\Users\bbhal\sh101\build-plugin-msvc\vst3_load_test.exe"
set "DLL=C:\Users\bbhal\VST3\OstraTorn101.vst3\Contents\x86_64-win\OstraTorn101.vst3"

set "SDKSRC=%SDK%\pluginterfaces\base\funknown.cpp %SDK%\pluginterfaces\base\coreiids.cpp %SDK%\pluginterfaces\base\conststringtable.cpp %SDK%\pluginterfaces\base\ustring.cpp"

echo == compiling load test ==
cl /nologo /std:c++17 /EHsc /W3 /I "%SDK%" "%SRC%" %SDKSRC% /Fe:"%OUT%" /link ole32.lib
if errorlevel 1 (
    echo == compile failed ==
    exit /b 1
)

echo.
echo == running load test ==
"%OUT%" "%DLL%"
endlocal
exit /b %errorlevel%
