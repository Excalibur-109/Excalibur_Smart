@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /std:c++20 /EHsc /c /I. /TP test_vec.cpp
