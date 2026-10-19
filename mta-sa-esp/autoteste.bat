@echo off
setlocal EnableExtensions DisableDelayedExpansion
title MTA SA - Autoteste isolado
if not exist "%~dp0injetar.bat" (
    echo ERRO: mantenha autoteste.bat ao lado de injetar.bat.
    pause
    exit /b 1
)
call "%~dp0injetar.bat" /autoteste
exit /b %ERRORLEVEL%
