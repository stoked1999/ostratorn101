#!/usr/bin/env bash
# Prints the state of the VST3 build attempt and the MSVC toolchain.
LOG=~/sh101/build-plugin-log.txt
echo "=== build log tail ==="
tail -6 "$LOG" 2>/dev/null
echo "=== counts ==="
printf 'errors: %s\n' "$(grep -c 'error:' "$LOG" 2>/dev/null)"
printf 'compile steps: %s\n' "$(grep -c 'Building CXX object' "$LOG" 2>/dev/null)"
printf 'log lines: %s\n' "$(wc -l < "$LOG" 2>/dev/null)"
echo "=== toolchain ==="
if [ -d "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC" ]; then
    echo "MSVC: present"
else
    echo "MSVC: absent"
fi
if [ -d "/c/Program Files (x86)/Windows Kits/10/Include" ]; then
    echo "Windows SDK: present"
else
    echo "Windows SDK: absent"
fi
