@echo off
setlocal EnableExtensions DisableDelayedExpansion
title MTA SA - Carregar ESP

set "ESP_BIN=%~dp0"
if exist "%ESP_BIN%mta-sa-loader.exe" goto localizar_arquivos
set "ESP_BIN=%~dp0bin\"
if exist "%ESP_BIN%mta-sa-loader.exe" goto localizar_arquivos
set "ESP_BIN=%~dp0build\Release\"
if exist "%ESP_BIN%mta-sa-loader.exe" goto localizar_arquivos
echo ERRO: mta-sa-loader.exe nao encontrado ao lado do BAT, em bin ou em build\Release.
set "ESP_RESULT=1"
goto terminar

:localizar_arquivos
if /i "%~1"=="/diagnosticar" goto diagnosticar
if /i "%~1"=="/autoteste" goto autoteste
if /i "%~1"=="/autoteste-hook" goto autoteste_hook
if /i "%~1"=="/verificar" goto conferir_arquivos
if exist "%ESP_BIN%pendente\mta-sa-esp.dll" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ESP_BIN%atualizar.ps1" -BinDirectory "%ESP_BIN%."
    if errorlevel 1 (
        set "ESP_RESULT=6"
        goto terminar
    )
)
:conferir_arquivos
if not exist "%ESP_BIN%mta-sa-esp.dll" (
    echo ERRO: mta-sa-esp.dll nao encontrada em "%ESP_BIN%".
    set "ESP_RESULT=2"
    goto terminar
)
if not exist "%ESP_BIN%mta-sa-esp.ini" (
    echo ERRO: mta-sa-esp.ini nao encontrado em "%ESP_BIN%".
    set "ESP_RESULT=3"
    goto terminar
)
if /i "%~1"=="/verificar" (
    echo Arquivos encontrados em "%ESP_BIN%".
    if exist "%ESP_BIN%pendente\mta-sa-esp.dll" echo Atualizacao pendente: feche o GTA/MTA e execute este BAT sem /verificar.
    echo Verificacao concluida. Nenhuma injecao executada.
    exit /b 0
)
pushd "%ESP_BIN%"
if errorlevel 1 (
    echo ERRO: nao foi possivel acessar a pasta dos binarios.
    set "ESP_RESULT=4"
    goto terminar
)
echo Se o GTA/MTA estiver fechado, ele sera aberto automaticamente.
echo Carregando mta-sa-esp.dll...
echo Metodo: hook de mensagens Windows.
echo.
"%ESP_BIN%mta-sa-loader.exe" --hook "%ESP_BIN%mta-sa-esp.dll"
set "ESP_RESULT=%ERRORLEVEL%"
popd
echo.
if not "%ESP_RESULT%"=="0" goto falha_carregador
echo Entrada pelo hook confirmada. Confirme a inicializacao do ESP no log:
echo "%ESP_BIN%mta-sa-esp.log"
echo.
echo INSERT: abre/fecha painel. Caixa Ativado: liga/desliga ESP. F5: recarrega INI. END: encerra.
goto terminar

:falha_carregador
echo O carregador retornou erro %ESP_RESULT%. Confira a mensagem acima.
echo Para consultar os privilegios sem injetar: injetar.bat /diagnosticar
goto terminar

:diagnosticar
"%ESP_BIN%mta-sa-loader.exe" --diagnosticar
set "ESP_RESULT=%ERRORLEVEL%"
goto terminar

:autoteste
"%ESP_BIN%mta-sa-loader.exe" --autoteste
set "ESP_RESULT=%ERRORLEVEL%"
goto terminar

:autoteste_hook
if not exist "%ESP_BIN%mta-sa-hook-checks.exe" (
    echo ERRO: mta-sa-hook-checks.exe nao encontrado. Recompile o projeto.
    set "ESP_RESULT=2"
    goto terminar
)
"%ESP_BIN%mta-sa-hook-checks.exe"
set "ESP_RESULT=%ERRORLEVEL%"
goto terminar

:terminar
if /i "%~1"=="/verificar" exit /b %ESP_RESULT%
echo.
pause
exit /b %ESP_RESULT%
