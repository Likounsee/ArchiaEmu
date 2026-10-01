@echo off
setlocal

echo ================================================
echo MyPS5Emu - COMPILATION CPU TEST
echo ================================================
echo.

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64

if errorlevel 1 (
    echo.
    echo [FAIL] VsDevCmd a echoue.
    exit /b 1
)

echo.
echo ===== CL VERSION =====
cl.exe

if errorlevel 1 (
    echo.
    echo [FAIL] cl.exe introuvable.
    exit /b 1
)

echo.
echo ===== COMPILATION =====

cd /d "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter"

cl.exe ^
 /nologo ^
 /std:c++20 ^
 /EHsc ^
 /W4 ^
 /I"C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\src" ^
 "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\tools\CpuAllFunctionsTest.cpp" ^
 "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\src\cpu\Cpu.cpp" ^
 "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\src\cpu\RegisterFile.cpp" ^
 "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\src\memory\Memory.cpp" ^
 /Fe:"C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\tools\CpuAllFunctionsTest.exe"

if errorlevel 1 (
    echo.
    echo [FAIL] Compilation C++.
    exit /b 1
)

echo.
echo [OK] Compilation C++ reussie.
echo.

if not exist "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\tools\CpuAllFunctionsTest.exe" (
    echo [FAIL] EXE non cree :
    echo "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\tools\CpuAllFunctionsTest.exe"
    exit /b 1
)

echo [OK] EXE cree :
echo "C:\Users\Likounsee\Downloads\PS5 emu\MyPS5Emu-starter\tools\CpuAllFunctionsTest.exe"

exit /b 0
