echo off
rem ### Compile txu.cpp to create txu.exe
rem ### Assumes Microsoft C++ compiler is installed and in the system PATH.

echo Building txu.exe from C++ source code.

cl /nologo /EHsc /Ox /W3 /MT txu.cpp
