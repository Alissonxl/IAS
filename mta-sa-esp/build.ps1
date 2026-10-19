param([string]$Compiler = '',[string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
if (-not $Compiler) {
    $portableCompiler = Join-Path $projectRoot '..\.tools\llvm-mingw-20261006-ucrt-x86_64\bin\i686-w64-mingw32-clang++.exe'
    if (Test-Path -LiteralPath $portableCompiler) { $Compiler = (Resolve-Path -LiteralPath $portableCompiler).Path }
}
if (-not $Compiler) {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) { throw 'Instale Visual Studio com C++ e CMake, ou informe -Compiler com LLVM-MinGW x86.' }
    & $cmake.Source -S $projectRoot -B (Join-Path $projectRoot 'build') -A Win32
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar CMake.' }
    & $cmake.Source --build (Join-Path $projectRoot 'build') --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar.' }
    & (Join-Path $projectRoot 'build\Release\mta-sa-checks.exe') (Join-Path $projectRoot 'mta-sa-esp.ini')
    if ($LASTEXITCODE -ne 0) { throw 'Verificacoes falharam.' }
    exit 0
}
$output = if ($OutputDirectory) { [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory)) } else { Join-Path $projectRoot 'bin' }
$objects = Join-Path $projectRoot 'build\objects'
New-Item -ItemType Directory -Path $output,$objects -Force | Out-Null
$common = @('-std=c++20','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-DUNICODE','-D_UNICODE','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0601','-I',(Join-Path $projectRoot 'src'))
$sources = @('src/dllmain.cpp','src/runtime.cpp','src/platform.cpp','src/config.cpp','src/game.cpp','src/render.cpp','src/separate_overlay.cpp','src/bones.cpp','src/mta_players.cpp','src/visibility.cpp','src/visual_settings.cpp','src/panel.cpp')
$dllObjects = @()
foreach ($source in $sources) {
    $object = Join-Path $objects (([IO.Path]::GetFileNameWithoutExtension($source))+'.o')
    & $Compiler @common -c (Join-Path $projectRoot $source) -o $object
    if ($LASTEXITCODE -ne 0) { throw "Falha ao compilar $source" }
    $dllObjects += $object
}
& $Compiler -shared -static @dllObjects -o (Join-Path $output 'mta-sa-esp.dll') -ld3d9 -luser32 -lgdi32 -lbcrypt -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'Falha ao vincular DLL.' }
$motionObjects = $dllObjects | Where-Object { [IO.Path]::GetFileName($_) -notin @('dllmain.o','runtime.o') }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\motion_checks.cpp') @motionObjects -o (Join-Path $output 'mta-sa-motion-checks.exe') -ld3d9 -luser32 -lgdi32 -lbcrypt -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacao de sincronizacao.' }
& (Join-Path $output 'mta-sa-motion-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Verificacao de sincronizacao falhou.' }

& $Compiler @common -static -municode (Join-Path $projectRoot 'loader\main.cpp') -o (Join-Path $output 'mta-sa-loader.exe') -ladvapi32 -lshell32 -luser32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar carregador.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\checks.cpp') (Join-Path $objects 'config.o') (Join-Path $objects 'game.o') (Join-Path $objects 'bones.o') (Join-Path $objects 'mta_players.o') (Join-Path $objects 'platform.o') (Join-Path $objects 'visibility.o') (Join-Path $objects 'visual_settings.o') -o (Join-Path $output 'mta-sa-checks.exe') -lbcrypt
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacoes.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\render_checks.cpp') (Join-Path $objects 'config.o') (Join-Path $objects 'render.o') (Join-Path $objects 'visibility.o') -o (Join-Path $output 'mta-sa-render-checks.exe') -ld3d9 -luser32 -lgdi32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacoes DX9.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\overlay_checks.cpp') (Join-Path $objects 'config.o') (Join-Path $objects 'render.o') (Join-Path $objects 'separate_overlay.o') (Join-Path $objects 'visibility.o') -o (Join-Path $output 'mta-sa-overlay-checks.exe') -ld3d9 -luser32 -lgdi32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacoes do overlay separado.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\panel_checks.cpp') (Join-Path $objects 'config.o') (Join-Path $objects 'visual_settings.o') (Join-Path $objects 'panel.o') -o (Join-Path $output 'mta-sa-panel-checks.exe') -luser32 -lgdi32 -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacoes do painel.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\panel_input_checks.cpp') (Join-Path $objects 'panel.o') (Join-Path $objects 'config.o') (Join-Path $objects 'visual_settings.o') -o (Join-Path $output 'mta-sa-panel-input-checks.exe') -luser32 -lgdi32 -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacoes de entrada do painel.' }

& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\hook_host.cpp') -o (Join-Path $output 'mta-sa-hook-host.exe') -luser32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar janela auxiliar.' }
& $Compiler @common -static -shared (Join-Path $projectRoot 'tests\hook_probe.cpp') -o (Join-Path $output 'mta-sa-hook-probe.dll') -luser32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar DLL auxiliar de hook.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\hook_checks.cpp') -o (Join-Path $output 'mta-sa-hook-checks.exe') -luser32
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacao de hook.' }
Copy-Item -LiteralPath (Join-Path $projectRoot 'mta-sa-esp-input.ini') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'mta-sa-esp.ini') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'carregar.cmd') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'injetar.bat') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'autoteste.bat') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'autoteste-hook.bat') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'atualizar.ps1') -Destination $output
& (Join-Path $output 'mta-sa-checks.exe') (Join-Path $projectRoot 'mta-sa-esp.ini')
if ($LASTEXITCODE -ne 0) { throw 'Verificacoes falharam.' }
& (Join-Path $output 'mta-sa-hook-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Verificacao de hook no auxiliar falhou.' }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot 'tests\update_checks.ps1') -WorkspaceRoot $projectRoot
if ($LASTEXITCODE -ne 0) { throw 'Verificacao da atualizacao pendente falhou.' }


& $Compiler @common -static (Join-Path $projectRoot 'tests\cursor_lease_checks.cpp') -o (Join-Path $output 'mta-sa-cursor-lease-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar verificacao de controle do cursor.' }
& (Join-Path $output 'mta-sa-cursor-lease-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Verificacao de controle do cursor falhou.' }



& $Compiler @common -static (Join-Path $projectRoot 'tests\aim_checks.cpp') -o (Join-Path $output 'mta-sa-aim-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes de mira.' }
& (Join-Path $output 'mta-sa-aim-checks.exe')
if ($LASTEXITCODE -ne 0) { throw 'Testes de mira falharam.' }
& $Compiler @common -static -municode (Join-Path $projectRoot 'tests\aim_panel_checks.cpp') (Join-Path $objects 'panel.o') (Join-Path $objects 'config.o') (Join-Path $objects 'visual_settings.o') -o (Join-Path $output 'mta-sa-aim-panel-checks.exe') -luser32 -lgdi32 -lgdiplus
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes da aba Mira.' }

Write-Output "Build x86 concluido: $output"
