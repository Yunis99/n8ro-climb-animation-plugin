@echo off
setlocal enabledelayedexpansion

REM ============================================================
REM sim-char-anim-custom-model - Open Solution Script
REM Calls dist setup scripts and opens sample solution
REM ============================================================

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR%"=="" (
    echo [Error] Failed to resolve script directory.
    exit /b 1
)

if "%SCRIPT_DIR:~-1%"=="\" (
    set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
)

set "N8RO_RELEASE_SETUP=%SCRIPT_DIR%\..\..\..\..\setup.cmd"
set "N8RO_RELEASE_DEV_SETUP=%SCRIPT_DIR%\..\..\..\setup-dev.cmd"
set "SOLUTION_FILE=%SCRIPT_DIR%\sim-char-anim-custom-model.slnx"

if not exist "%N8RO_RELEASE_SETUP%" (
    echo [Error] dist setup script not found: %N8RO_RELEASE_SETUP%
    exit /b 1
)

if not exist "%N8RO_RELEASE_DEV_SETUP%" (
    echo [Error] dev setup script not found: %N8RO_RELEASE_DEV_SETUP%
    exit /b 1
)

if not exist "%SOLUTION_FILE%" (
    echo [Error] Solution file not found: %SOLUTION_FILE%
    exit /b 1
)

call "%N8RO_RELEASE_SETUP%"
if errorlevel 1 (
    echo [Error] setup.cmd failed.
    exit /b 1
)

if not defined N8RO_SETUP_DONE (
    echo [Error] N8RO_SETUP_DONE is not set after setup.cmd.
    exit /b 1
)

if not defined N8RO_RELEASE (
    echo [Error] N8RO_RELEASE is not set after setup.cmd.
    exit /b 1
)

call "%N8RO_RELEASE_DEV_SETUP%"
if errorlevel 1 (
    echo [Error] setup-dev.cmd failed.
    exit /b 1
)

if not defined VS_INSIDERS_EXE (
    echo [Error] Visual Studio Insiders executable not found.
    echo [Error] Set VS_INSIDERS_EXE to a valid devenv.exe path, or install VS 2022 Insiders.
    exit /b 1
)

if not exist "%VS_INSIDERS_EXE%" (
    echo [Error] Visual Studio Insiders path is invalid: %VS_INSIDERS_EXE%
    exit /b 1
)

echo [OK] Opening solution with: %VS_INSIDERS_EXE%
start "" "%VS_INSIDERS_EXE%" "%SOLUTION_FILE%"
if errorlevel 1 (
    echo [Error] Failed to launch Visual Studio Insiders.
    exit /b 1
)

echo [SUCCESS] sim-char-anim-custom-model solution launch command sent.
exit /b 0
