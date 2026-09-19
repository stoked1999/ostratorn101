@echo off
rem Verifies the built VST3 bundle: structure + exported factory entry point.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul

set "BUNDLE=C:\Users\bbhal\VST3\SH-101.vst3"
echo == bundle contents ==
dir /s /b "%BUNDLE%"

echo.
echo == exported factory entry point ==
dumpbin /exports "%BUNDLE%\Contents\x86_64-win\SH-101.vst3" | findstr /i "GetPluginFactory GetPluginFactory2"
endlocal
exit /b 0
