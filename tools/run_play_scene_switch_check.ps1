param([string]$EditorExe = "")
$ErrorActionPreference = 'Stop'
$repoPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$EditorExe) { $EditorExe = Join-Path $repoPath 'x64/Debug/IvyEngine.exe' }
if (!(Test-Path -LiteralPath $EditorExe -PathType Leaf)) { throw 'Build the Debug x64 Editor first.' }
$logPath = Join-Path $PSScriptRoot 'phase6-test-build/play-switch'
New-Item -ItemType Directory -Path $logPath -Force | Out-Null
$previousMode = $env:IVY_PLAY_SCENE_SWITCH_CHECK
$previousCapture = $env:IVY_FRAME_CAPTURE
try {
    $env:IVY_FRAME_CAPTURE = $null
    foreach ($mode in @('fullscreen','viewport')) {
        $env:IVY_PLAY_SCENE_SWITCH_CHECK = $mode
        $stdout = Join-Path $logPath "$mode.log"
        $stderr = Join-Path $logPath "$mode.err.log"
        $process = Start-Process -FilePath $EditorExe -WorkingDirectory $env:TEMP -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        if (!$process.WaitForExit(30000)) { throw "Timed out: $mode, PID $($process.Id). See $logPath" }
        $process.Refresh()
        if ($process.ExitCode -ne 0 -or !(Select-String -LiteralPath $stdout -Pattern '^PLAY_SCENE_SWITCH_CHECK PASS:' -Quiet)) {
            Get-Content -LiteralPath $stderr -Tail 10
            throw "Failed: $mode, exit $($process.ExitCode). Open TestProject's scene_topdown_2d.json in Editor before running this check."
        }
        Get-Content -LiteralPath $stdout | Select-String '^PLAY_SCENE_SWITCH_CHECK PASS:' | ForEach-Object { $_.Line }
    }
} finally {
    $env:IVY_PLAY_SCENE_SWITCH_CHECK = $previousMode
    $env:IVY_FRAME_CAPTURE = $previousCapture
}
