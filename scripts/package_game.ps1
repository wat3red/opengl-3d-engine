param(
    [string]$GameName = "Doom",
    [string]$StartingScene = "res/scenes/default.scene",
    [string]$OutputDir = "dist/Doom",
    [int]$Width = 1600,
    [int]$Height = 900,
    [switch]$Fullscreen = $false,
    [switch]$NoRebuild = $false,
    [switch]$NoZip = $false
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\.."

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Ignis Engine - Standalone Distribution Packager" -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Game Name:      $GameName"
Write-Host "Starting Scene: $StartingScene"
Write-Host "Output Dir:     $OutputDir"
Write-Host "Target Res:     ${Width}x${Height} (Fullscreen: $($Fullscreen.IsPresent))"
Write-Host ""

$msbuild = "D:\Visual Studio Community 2026\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -find MSBuild\**\Bin\MSBuild.exe
        if ($found) { $msbuild = $found }
    }
}

if (-not $NoRebuild) {
    Write-Host "[1/5] Compiling Game Scripts (Release x64)..." -ForegroundColor Green
    & $msbuild "$root\projects\Doom\DoomGame.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal /m
    if ($LASTEXITCODE -ne 0) { throw "Failed to build Game.dll" }

    Write-Host "[2/5] Compiling Engine Runtime (Release x64)..." -ForegroundColor Green
    & $msbuild "$root\Ignis.vcxproj" /p:Configuration=Release /p:Platform=x64 /v:minimal /m
    if ($LASTEXITCODE -ne 0) { throw "Failed to build Ignis.exe" }
}

$destDir = "$root\$OutputDir"
Write-Host "[3/5] Assembling Standalone Package at '$destDir'..." -ForegroundColor Green
if (Test-Path $destDir) { Remove-Item $destDir -Recurse -Force }
New-Item -ItemType Directory -Path $destDir -Force | Out-Null
New-Item -ItemType Directory -Path "$destDir\shaders" -Force | Out-Null
New-Item -ItemType Directory -Path "$destDir\res" -Force | Out-Null

# Copy Game Executable
$srcExe = "$root\x64\Release\Ignis.exe"
if (-not (Test-Path $srcExe)) { $srcExe = "$root\Ignis.exe" }
$targetExeName = if ($GameName.EndsWith(".exe")) { $GameName } else { "$GameName.exe" }
Copy-Item $srcExe "$destDir\$targetExeName" -Force
Write-Host "  -> Packaged Executable: $targetExeName"

# Copy Game.dll
Copy-Item "$root\Game.dll" "$destDir\Game.dll" -Force
Write-Host "  -> Packaged Game Module: Game.dll"

# Copy Shaders
Copy-Item "$root\shaders\*" "$destDir\shaders" -Recurse -Force
Write-Host "  -> Packaged Shaders"

# Copy Res (excluding backups)
Get-ChildItem -Path "$root\res" | ForEach-Object {
    if ($_.Name -ne "backups") {
        Copy-Item $_.FullName "$destDir\res" -Recurse -Force
    }
}
Write-Host "  -> Packaged Assets (Models, Audio, Scenes, Textures)"

# Write game.json Manifest
$manifest = @{
    title         = $GameName
    startingScene = $StartingScene
    gameModule    = "Game.dll"
    windowWidth   = $Width
    windowHeight  = $Height
    fullscreen    = $Fullscreen.IsPresent
    vsync         = $true
    standalone    = $true
} | ConvertTo-Json -Depth 4

Set-Content -Path "$destDir\game.json" -Value $manifest -Encoding UTF8
Write-Host "  -> Generated Launch Manifest: game.json"

# Package ZIP
if (-not $NoZip) {
    Write-Host "[4/5] Compressing ZIP Distribution Archive..." -ForegroundColor Green
    $zipName = "${GameName}_v1.0_Windows_x64.zip"
    $zipPath = "$root\dist\$zipName"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path "$destDir\*" -DestinationPath $zipPath -Force
    $zipSizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
    Write-Host "  -> Created Archive: $zipPath ($zipSizeMb MB)" -ForegroundColor Yellow
}

$allFiles = Get-ChildItem -Path $destDir -Recurse -File
$totalSizeMb = [math]::Round(($allFiles | Measure-Object -Property Length -Sum).Sum / 1MB, 2)

Write-Host ""
Write-Host "[5/5] Standalone Distribution Successfully Packaged!" -ForegroundColor Cyan
Write-Host "  Total Files:  $($allFiles.Count)"
Write-Host "  Total Size:   $totalSizeMb MB"
Write-Host "  Target Path:  $destDir"
Write-Host "  Executable:   $destDir\$targetExeName"
Write-Host "==========================================================" -ForegroundColor Cyan
