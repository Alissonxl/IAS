param([Parameter(Mandatory=$true)][string]$BinDirectory)
$ErrorActionPreference='Stop'
$binaryRoot=[IO.Path]::GetFullPath($BinDirectory)
$pendingRoot=Join-Path $binaryRoot 'pendente'
$pendingDll=Join-Path $pendingRoot 'mta-sa-esp.dll'
if (-not (Test-Path -LiteralPath $pendingDll)) { exit 0 }
$pendingIni=Join-Path $pendingRoot 'mta-sa-esp.ini'
$currentDll=Join-Path $binaryRoot 'mta-sa-esp.dll'
$currentIni=Join-Path $binaryRoot 'mta-sa-esp.ini'
$dllApplied=$false
$iniApplied=$false
$workRoot=$null
$hadDll=Test-Path -LiteralPath $currentDll
$hadIni=Test-Path -LiteralPath $currentIni
try {
    if (-not (Test-Path -LiteralPath $pendingIni)) { throw 'INI da atualizacao ausente.' }
    # Check both destinations before replacing either member of the pair.
    foreach ($currentFile in @($currentDll,$currentIni)) {
        if (Test-Path -LiteralPath $currentFile) {
            $probe=[IO.File]::Open($currentFile,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
            $probe.Dispose()
        }
    }
    $workRoot=Join-Path $binaryRoot ('atualizacao-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $workRoot | Out-Null
    $stagedDll=Join-Path $workRoot 'mta-sa-esp.dll.new'
    $stagedIni=Join-Path $workRoot 'mta-sa-esp.ini.new'
    $backupDll=Join-Path $workRoot 'mta-sa-esp.dll.old'
    $backupIni=Join-Path $workRoot 'mta-sa-esp.ini.old'
    Copy-Item -LiteralPath $pendingDll -Destination $stagedDll
    Copy-Item -LiteralPath $pendingIni -Destination $stagedIni
    foreach ($pair in @(@($pendingDll,$stagedDll),@($pendingIni,$stagedIni))) {
        if ((Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash) {
            throw 'Arquivo pendente mudou durante a preparacao. Tente novamente.'
        }
    }
    if ($hadDll) { [IO.File]::Replace($stagedDll,$currentDll,$backupDll,$true) }
    else { [IO.File]::Move($stagedDll,$currentDll) }
    $dllApplied=$true
    if ($hadIni) { [IO.File]::Replace($stagedIni,$currentIni,$backupIni,$true) }
    else { [IO.File]::Move($stagedIni,$currentIni) }
    $iniApplied=$true
} catch {
    $failure=$_.Exception.Message
    $rollbackFailed=$false
    # A second-file failure restores the first file; originals remain in the backup directory.
    foreach ($item in @(@($iniApplied,$hadIni,$currentIni,'mta-sa-esp.ini.old'),@($dllApplied,$hadDll,$currentDll,'mta-sa-esp.dll.old'))) {
        if (-not $item[0]) { continue }
        try {
            if ($item[1]) {
                $restore=Join-Path $workRoot ($item[3]+'.restore')
                Copy-Item -LiteralPath (Join-Path $workRoot $item[3]) -Destination $restore
                [IO.File]::Replace($restore,$item[2],$null,$true)
            } else { Remove-Item -LiteralPath $item[2] -Force }
        } catch { $rollbackFailed=$true; Write-Output ('Falha ao restaurar '+$item[2]+': '+$_.Exception.Message) }
    }
    Write-Output 'Atualizacao nao aplicada. Libere os arquivos e execute injetar.bat novamente.'
    Write-Output $failure
    if ($rollbackFailed) { Write-Output ('Recuperacao manual necessaria. Backups preservados em '+$workRoot) }
    elseif ($dllApplied -or $iniApplied) { Write-Output 'Arquivos anteriores restaurados; atualizacao pendente preservada.' }
    exit 6
}
try { Remove-Item -LiteralPath $pendingDll,$pendingIni -Force }
catch { Write-Output ('Atualizacao aplicada, mas os arquivos pendentes nao puderam ser removidos: '+$_.Exception.Message) }
Write-Output 'Atualizacao aplicada: novo painel escuro, INSERT abre/fecha e caixa Ativado controla ESP.'
Write-Output ('Backups dos arquivos anteriores: '+$workRoot)
exit 0
