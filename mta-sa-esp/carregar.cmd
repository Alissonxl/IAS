@echo off
cd /d "%~dp0"
if not exist "%~dp0mta-sa-loader.exe" (
    echo Execute carregar.cmd dentro da pasta bin depois de compilar.
    pause
    exit /b 1
)
"%~dp0mta-sa-loader.exe" --hook "%~dp0mta-sa-esp.dll"
pause
