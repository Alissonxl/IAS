@echo off
setlocal EnableExtensions DisableDelayedExpansion
title MTA SA - Autoteste do hook
if not exist "%~dp0injetar.bat" (
    echo ERRO: mantenha autoteste-hook.bat ao lado de injetar.bat.
    pause
    exit /b 1
)
call "%~dp0injetar.bat" /autoteste-hook
exit /b %ERRORLEVEL%
