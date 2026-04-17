param(
  [ValidateSet('Debug','Release')]
  [string]$Config = 'Debug',

  [string]$Triplet = 'x64-windows',

  [string]$BuildDir = 'build_sgg_vcpkg',

  [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Step([string]$msg) {
  Write-Host "-> $msg"
}

function Invoke-External {
  param(
    [Parameter(Mandatory = $true)][string]$Label,
    [Parameter(Mandatory = $true)][scriptblock]$Command
  )

  Write-Step $Label
  $global:LASTEXITCODE = 0
  & $Command
  if ($LASTEXITCODE -ne 0) {
    throw "$Label failed (exit code $LASTEXITCODE)"
  }
}

function Get-CMakeCacheValue {
  param(
    [Parameter(Mandatory = $true)][string]$CachePath,
    [Parameter(Mandatory = $true)][string]$Key
  )

  if (!(Test-Path -LiteralPath $CachePath)) { return $null }
  $line = Select-String -LiteralPath $CachePath -Pattern ("^" + [regex]::Escape($Key) + ":") -SimpleMatch -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $line) { return $null }
  $parts = $line.Line.Split('=', 2)
  if ($parts.Count -lt 2) { return $null }
  return $parts[1]
}

$root = (Resolve-Path -LiteralPath "$PSScriptRoot\..").Path
Set-Location -LiteralPath $root

Write-Step "Project root: $root"

# Ensure winget command aliases (ffmpeg/ffplay/ffprobe) are visible in this shell.
$wingetLinks = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'
if ((Test-Path -LiteralPath $wingetLinks) -and ($env:Path -notlike "*$wingetLinks*")) {
  $env:Path = "$wingetLinks;$env:Path"
}

# 1) Έλεγχος ότι υπάρχει vcpkg
$bootstrap = Join-Path $root 'vcpkg\bootstrap-vcpkg.bat'
$vcpkgExe  = Join-Path $root 'vcpkg\vcpkg.exe'
if (!(Test-Path -LiteralPath $vcpkgExe)) {
  if (!(Test-Path -LiteralPath $bootstrap)) {
    throw "vcpkg bootstrap not found: $bootstrap"
  }
  Invoke-External -Label 'Bootstrapping vcpkg...' -Command { & $bootstrap | Out-Host }
}

# 2) Εγκατάσταση εξαρτήσεων για SGG
Invoke-External -Label 'Installing SGG deps (glew, glm, sdl2, sdl2-mixer, freetype)...' -Command {
  & $vcpkgExe install glew glm sdl2 sdl2-mixer freetype --triplet $Triplet | Out-Host
}

# 3) CMake configure 
$toolchain = Join-Path $root 'vcpkg\scripts\buildsystems\vcpkg.cmake'
if (!(Test-Path -LiteralPath $toolchain)) {
  throw "vcpkg toolchain not found: $toolchain"
}

# Το vcpkg toolchain διαβάζει και env vars
$env:VCPKG_ROOT = (Join-Path $root 'vcpkg')
$env:VCPKG_TARGET_TRIPLET = $Triplet

if (Test-Path -LiteralPath $BuildDir) {
  $cachePath = Join-Path $root "$BuildDir\CMakeCache.txt"
  if (Test-Path -LiteralPath $cachePath) {
    $cachedToolchain = Get-CMakeCacheValue -CachePath $cachePath -Key 'CMAKE_TOOLCHAIN_FILE'
    $cachedTriplet   = Get-CMakeCacheValue -CachePath $cachePath -Key 'VCPKG_TARGET_TRIPLET'

    if ($cachedTriplet -and $cachedTriplet.StartsWith('$')) {
      Write-Host "Warning: broken cached VCPKG_TARGET_TRIPLET='$cachedTriplet' detected. Forcing -Reconfigure." -ForegroundColor Yellow
      $Reconfigure = $true
    }

    if ($cachedToolchain -and ($cachedToolchain -ne $toolchain) -and (-not $Reconfigure)) {
      Write-Host "Warning: this build folder was configured with a different toolchain." -ForegroundColor Yellow
      Write-Host "         Re-run with -Reconfigure to avoid cache issues." -ForegroundColor Yellow
    }
  }
}

if ($Reconfigure -and (Test-Path -LiteralPath $BuildDir)) {
  Write-Step "Removing build folder: $BuildDir"
  Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

Invoke-External -Label "Running CMake configure in '$BuildDir'..." -Command {
  $cmakeArgs = @(
    '-S', '.',
    '-B', $BuildDir,
    '-DWITH_SGG=ON',
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DVCPKG_TARGET_TRIPLET=$Triplet"
  )
  & cmake @cmakeArgs
}

# 4) Build
Invoke-External -Label "Building target run_sim_sgg ($Config)..." -Command {
  cmake --build $BuildDir --config $Config --target run_sim_sgg
}

# 5) Run (αν μείνει ανοιχτό μπορεί να μπλοκάρει rebuild την επόμενη φορά)
$exe = Join-Path $root "$BuildDir\$Config\run_sim_sgg.exe"
if (!(Test-Path -LiteralPath $exe)) {
  throw "Built exe not found: $exe"
}

Write-Step "Launching: $exe"
Start-Process -FilePath $exe
