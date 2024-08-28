@echo off
echo Removing test files and build output files.
if exist txu.exe del txu.exe
if exist txu.obj del txu.obj
if exist txu.pdb del txu.pdb
if exist __out.* del __out.*
