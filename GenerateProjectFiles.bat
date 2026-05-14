@echo off
setlocal

set "UE_ROOT=E:\EpicDownload\UE_5.4"
set "UPROJECT=%~dp0VoxelWorld.uproject"
set "UBT=%UE_ROOT%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"

if not exist "%UBT%" (
    echo [ERROR] UnrealBuildTool not found at: %UBT%
    pause
    exit /b 1
)

if not exist "%UPROJECT%" (
    echo [ERROR] uproject not found at: %UPROJECT%
    pause
    exit /b 1
)

echo Generating Visual Studio project files...
echo Engine: %UE_ROOT%
echo Project: %UPROJECT%
echo.

"%UBT%" -projectfiles -project="%UPROJECT%" -game -rocket -progress

if errorlevel 1 (
    echo.
    echo [ERROR] Project file generation failed.
    pause
    exit /b 1
)

echo.
echo [OK] Done. VoxelWorld.sln should now exist in this directory.
pause
