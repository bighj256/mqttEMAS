@echo off
setlocal

REM ==========================================
REM Qt Debug Build Script (MinGW)
REM ==========================================

REM 1. Set environment variables (Modify these paths to match your Qt and MinGW installation)
REM Based on your project config, it uses Qt 6.9.1 MinGW 64-bit
set QT_DIR=H:\Qt\6.9.1\mingw_64
set MINGW_DIR=H:\Qt\Tools\mingw1310_64

REM If qmake and mingw32-make are already in your system PATH, you can comment out the line below
set PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%

REM 2. Set project and build directory
set PRO_FILE=mqttEMAS2.pro
set BUILD_DIR=build-debug

echo ==========================================
echo Starting Qt Debug Build...
echo Project File: %PRO_FILE%
echo ==========================================

REM 3. Check if qmake is available
where qmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] qmake not found. Please check the QT_DIR path at the top of the script.
    pause
    exit /b 1
)

REM 4. Check if mingw32-make is available
where mingw32-make >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] mingw32-make not found. Please check the MINGW_DIR path at the top of the script.
    pause
    exit /b 1
)

REM 5. Create and enter the shadow build directory (similar to QtCreator's behavior)
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
cd %BUILD_DIR%

REM 6. Run qmake to generate Makefile
echo [1/2] Running qmake...
qmake ../%PRO_FILE% -spec win32-g++ "CONFIG+=debug" "CONFIG+=qml_debug"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] qmake failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

REM 7. Run mingw32-make to compile
echo [2/2] Running mingw32-make...
mingw32-make -j%NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    cd ..
    pause
    exit /b %ERRORLEVEL%
)

echo ==========================================
echo Build successful! The generated exe file is usually located in the %BUILD_DIR%\debug directory.
echo ==========================================

REM Return to the original directory
cd ..
endlocal
pause
