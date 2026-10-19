param([Parameter(Mandatory=$true)][string]$WorkspaceRoot)
$ErrorActionPreference='Stop'
$taskWorkspace=[IO.Path]::GetFullPath($WorkspaceRoot)
$fixture=Join-Path $taskWorkspace ('build\update-checks-'+[guid]::NewGuid().ToString('N'))
$updater=Join-Path $taskWorkspace 'atualizar.ps1'
New-Item -ItemType Directory -Path $fixture,(Join-Path $fixture 'pendente') -Force | Out-Null
function Assert-Check([bool]$condition,[string]$label) {
    if (-not $condition) { throw "FAIL $label" }
    Write-Output "PASS $label"
}
function Set-Fixture {
    Set-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.dll') -Value 'DLL-old' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.ini') -Value 'INI-old' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.dll') -Value 'DLL-new' -Encoding ascii
    Set-Content -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.ini') -Value 'INI-new' -Encoding ascii
}
Set-Fixture
$locked=[IO.File]::Open((Join-Path $fixture 'mta-sa-esp.ini'),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $updater -BinDirectory $fixture | Out-Null
    $updateCode=$LASTEXITCODE
} finally { $locked.Dispose() }
Assert-Check ($updateCode -eq 6) 'locked INI rejects update before replacing the DLL'
Assert-Check (((Get-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.dll') -Raw).Trim()) -eq 'DLL-old') 'failed update preserves the original DLL'
Assert-Check ((Test-Path -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.dll')) -and (Test-Path -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.ini'))) 'failed update preserves both pending files'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $updater -BinDirectory $fixture | Out-Null
Assert-Check ($LASTEXITCODE -eq 0) 'unlocked pair updates successfully'
Assert-Check (((Get-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.dll') -Raw).Trim()) -eq 'DLL-new') 'new DLL installed'
Assert-Check (((Get-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.ini') -Raw).Trim()) -eq 'INI-new') 'new profile installed'
$backup=Get-ChildItem -LiteralPath $fixture -Directory -Filter 'atualizacao-*' | Select-Object -First 1
Assert-Check ($null -ne $backup -and (Test-Path -LiteralPath (Join-Path $backup.FullName 'mta-sa-esp.dll.old')) -and (Test-Path -LiteralPath (Join-Path $backup.FullName 'mta-sa-esp.ini.old'))) 'previous pair preserved as backups'
Assert-Check (-not (Test-Path -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.dll'))) 'applied pending DLL removed'
Set-Fixture
Remove-Item -LiteralPath (Join-Path $fixture 'pendente\mta-sa-esp.ini')
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $updater -BinDirectory $fixture | Out-Null
Assert-Check ($LASTEXITCODE -eq 6 -and ((Get-Content -LiteralPath (Join-Path $fixture 'mta-sa-esp.dll') -Raw).Trim()) -eq 'DLL-old') 'missing pending profile leaves installed pair unchanged'
