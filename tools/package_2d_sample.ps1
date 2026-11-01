param(
    [string]$ContentRoot = "",
    [string]$Destination = "",
    [ValidateSet("Debug", "Release")][string]$Configuration = "Debug",
    [string]$DefaultScene = "scene_topdown_2d.json"
)
$ErrorActionPreference = 'Stop'
$engineRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$contentPath = if ($ContentRoot) { [IO.Path]::GetFullPath($ContentRoot) } else { [IO.Path]::GetFullPath((Join-Path $engineRoot "../TestProject")) }
if (!$Destination) { $Destination = Join-Path $contentPath 'deploy_phase6' }
$destinationPath = [IO.Path]::GetFullPath($Destination)
$engineOutput = Join-Path $engineRoot "x64/$Configuration"
foreach ($protectedPath in @($contentPath, (Join-Path $contentPath 'rom'), (Join-Path $contentPath 'game'), $engineOutput)) {
    if ($destinationPath.Equals($protectedPath, [StringComparison]::OrdinalIgnoreCase)) { throw 'Choose a separate package directory.' }
}
foreach ($protectedTree in @((Join-Path $contentPath 'rom'), (Join-Path $contentPath 'game'))) {
    if ($destinationPath.StartsWith($protectedTree + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Package destination is inside source assets.' }
}
if ([IO.Path]::GetFileName($DefaultScene) -ne $DefaultScene -or !(Test-Path -LiteralPath (Join-Path $contentPath "rom/scene/$DefaultScene"))) { throw 'DefaultScene must name an existing scene.' }
# Match the editor package layout; overlay freshly compiled shaders and branding.
New-Item -ItemType Directory -Path $destinationPath -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $engineOutput 'IvyEngineRuntime.exe') -Destination (Join-Path $destinationPath 'Ivy2DSample.exe') -Force
Copy-Item -LiteralPath (Join-Path $engineOutput 'libfbxsdk.dll') -Destination $destinationPath -Force
Copy-Item -LiteralPath (Join-Path $contentPath 'rom') -Destination $destinationPath -Recurse -Force
foreach ($folder in @('shader','branding')) {
    $sourceFolder = Join-Path $engineOutput "resources/$folder"
    if (!(Test-Path -LiteralPath $sourceFolder)) { $sourceFolder = Join-Path $engineRoot "resources/$folder" }
    $targetFolder = Join-Path $destinationPath "rom/$folder"
    New-Item -ItemType Directory -Path $targetFolder -Force | Out-Null
    if (Test-Path -LiteralPath $sourceFolder) {
        Get-ChildItem -LiteralPath $sourceFolder -File | Copy-Item -Destination $targetFolder -Force
    }
}
$materialize = Join-Path $PSScriptRoot 'materialize_engine_defaults.py'
python $materialize (Join-Path $destinationPath 'rom') (Join-Path $engineRoot 'resources') (Join-Path $destinationPath 'rom')
if ($LASTEXITCODE -ne 0) { throw 'Failed to materialize referenced engine defaults.' }
$licenseDestination = Join-Path $destinationPath 'licenses'
New-Item -ItemType Directory -Path $licenseDestination -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $engineRoot 'libraries/box2d/LICENSE') -Destination (Join-Path $licenseDestination 'Box2D.txt') -Force
$scriptDestination = Join-Path $destinationPath 'game/scripts/bin'
New-Item -ItemType Directory -Path $scriptDestination -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $contentPath 'game/scripts/bin/GameScripts.dll') -Destination $scriptDestination -Force
$project = Get-Content -LiteralPath (Join-Path $contentPath 'project.json') -Raw | ConvertFrom-Json
$project.defaultScene = $DefaultScene
$project.lastOpenedScene = $DefaultScene
$project | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $destinationPath 'project.json') -Encoding utf8
# Runtime also reads default_scene.txt; keep it consistent without changing content sources.
Set-Content -LiteralPath (Join-Path $destinationPath 'rom/scene/default_scene.txt') -Value $DefaultScene -Encoding ascii
Write-Output $destinationPath
