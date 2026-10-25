param([string]$EditorExe = "")
$ErrorActionPreference = 'Stop'
$repoPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$EditorExe) { $EditorExe = Join-Path $repoPath 'x64/Debug/IvyEngine.exe' }
$logPath = Join-Path $PSScriptRoot 'phase6-test-build/scene-hotload'
New-Item -ItemType Directory -Path $logPath -Force | Out-Null
$previous = $env:IVY_SCENE_HOT_LOAD_CHECK
$previousCapture = $env:IVY_FRAME_CAPTURE
try {
    $env:IVY_SCENE_HOT_LOAD_CHECK = '1'
    $env:IVY_FRAME_CAPTURE = $null
    $stdout = Join-Path $logPath 'check.log'
    $stderr = Join-Path $logPath 'check.err.log'
    $process = Start-Process -FilePath $EditorExe -WorkingDirectory $env:TEMP -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    if (!$process.WaitForExit(120000)) { throw "Timed out: PID $($process.Id). See $logPath" }
    $process.Refresh()
    if ($process.ExitCode -ne 0 -or !(Select-String -LiteralPath $stdout -Pattern '^SCENE_HOT_LOAD_CHECK PASS:' -Quiet)) {
        Get-Content -LiteralPath $stderr -Tail 10
        throw "Scene hot-load check failed: exit $($process.ExitCode). Open an existing project first."
    }
    Get-Content -LiteralPath $stdout | Select-String '^SCENE_HOT_LOAD_CHECK PASS:' | ForEach-Object { $_.Line }
} finally {
    $env:IVY_SCENE_HOT_LOAD_CHECK = $previous
    $env:IVY_FRAME_CAPTURE = $previousCapture
}
