@echo off
echo Running tests.

if exist __out.* del __out.*

echo Testing ANSI.
txu /O=ANSI    ansitext.txt __out.txt

echo Testing UTF-8.
txu /O=UTF8    ansitext.txt __out.utf8

echo Testing UTF-16.
txu /O=UTF16   ansitext.txt __out.utf16

echo Testing UTF-16BE.
txu /O=UTF16BE ansitext.txt __out.utf16be

