@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\package_game.ps1" %*
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Packaging failed with error code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo Packaging completed successfully.
