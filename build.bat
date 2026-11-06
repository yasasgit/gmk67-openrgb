@echo off
setlocal

rem Qt 6.8.3 MSVC 64-bit, installed with:
rem   pip install aqtinstall
rem   aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O C:\Qt
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.8.3\msvc2022_64

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VS_DIR=%%i
if "%VS_DIR%"=="" (
    echo Visual Studio 2022 Build Tools with "Desktop development with C++" not found.
    exit /b 1
)
call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

cd /d "%~dp0"

if not exist OpenRGB (
    git clone --filter=blob:none --no-checkout https://gitlab.com/CalcProgrammer1/OpenRGB.git OpenRGB || exit /b 1
    git -C OpenRGB checkout 8e16de27858cf2be6af54a33ce003df74cb9fd3e || exit /b 1
)

cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%" || exit /b 1
cmake --build build || exit /b 1

if "%OUT_DIR%"=="" set OUT_DIR=%USERPROFILE%\Downloads
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
copy /y build\OpenRGBGMK67Plugin.dll "%OUT_DIR%\" || exit /b 1

echo.
echo Built: %OUT_DIR%\OpenRGBGMK67Plugin.dll
echo Copy it to %APPDATA%\OpenRGB\plugins, close the stock GMK67 app, then restart OpenRGB.
