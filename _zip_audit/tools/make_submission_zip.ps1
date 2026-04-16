param(
  [Parameter(Mandatory = $true)]
  [string]$ZipName,

  [string]$StagingDir = "submission_staging"
)

$ErrorActionPreference = "Stop"

function Copy-TreeFiltered {
  param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Dest,
    [string[]]$Exclude = @()
  )

  if (!(Test-Path -LiteralPath $Source)) {
    Write-Host "Missing, skipping: $Source"
    return
  }

  New-Item -ItemType Directory -Force -Path $Dest | Out-Null

  $args = @(
    "`"$Source`"",
    "`"$Dest`"",
    "/E",
    "/NFL", "/NDL", "/NJH", "/NJS", "/NP",
    "/R:1", "/W:1"
  )

  foreach ($ex in $Exclude) {
    $args += @("/XD", $ex)
    $args += @("/XF", $ex)
  }

  $null = & robocopy @args
  # Το robocopy επιστρέφει "bitmask" κωδικούς· 0-7 θεωρούνται επιτυχία
  if ($LASTEXITCODE -gt 7) {
    throw "robocopy failed for $Source -> $Dest (code $LASTEXITCODE)"
  }
}

$root = (Resolve-Path -LiteralPath $PSScriptRoot\..).Path
Set-Location -LiteralPath $root

$staging = Join-Path $root $StagingDir
if (Test-Path -LiteralPath $staging) {
  Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null

# Copy only sources/assets/docs
Copy-Item -LiteralPath (Join-Path $root "CMakeLists.txt") -Destination $staging -Force
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $staging -Force

Copy-TreeFiltered -Source (Join-Path $root "docs")     -Dest (Join-Path $staging "docs") -Exclude @(
  "assignment.pdf",
  "assignment_extracted_full.txt",
  "assignment_extracted_snippets.txt"
)
Copy-TreeFiltered -Source (Join-Path $root "grid")     -Dest (Join-Path $staging "grid")
Copy-TreeFiltered -Source (Join-Path $root "include")  -Dest (Join-Path $staging "include")
Copy-TreeFiltered -Source (Join-Path $root "src")      -Dest (Join-Path $staging "src")
Copy-TreeFiltered -Source (Join-Path $root "examples") -Dest (Join-Path $staging "examples")
Copy-TreeFiltered -Source (Join-Path $root "configs")  -Dest (Join-Path $staging "configs")
Copy-TreeFiltered -Source (Join-Path $root "maps")     -Dest (Join-Path $staging "maps")

# Optional build/run scripts (without helper tools).
$toolsSrc = Join-Path $root "tools"
$toolsDst = Join-Path $staging "tools"
Copy-TreeFiltered -Source $toolsSrc -Dest $toolsDst -Exclude @(
  "extract_assignment_pdf.py"
)

if (Test-Path -LiteralPath (Join-Path $root "assets")) {
  Copy-TreeFiltered -Source (Join-Path $root "assets") -Dest (Join-Path $staging "assets")
}

# Include ONLY the 2 required SGG headers.
$sggSrc = Join-Path $root "sgg-main\sgg"
$sggDst = Join-Path $staging "sgg"
New-Item -ItemType Directory -Force -Path $sggDst | Out-Null

$graphicsH = Join-Path $sggSrc "graphics.h"
$scancodesH = Join-Path $sggSrc "scancodes.h"
if (!(Test-Path -LiteralPath $graphicsH) -or !(Test-Path -LiteralPath $scancodesH)) {
  throw "SGG headers not found under $sggSrc (expected graphics.h and scancodes.h)."
}
Copy-Item -LiteralPath $graphicsH -Destination (Join-Path $sggDst "graphics.h") -Force
Copy-Item -LiteralPath $scancodesH -Destination (Join-Path $sggDst "scancodes.h") -Force


$junkPatterns = @(
  "*.pdb", "*.obj", "*.ilk", "*.idb", "*.ipch", "*.tlog", "*.pch", "*.tmp",
  "*.exe", "*.dll",
  ".vs", "CMakeFiles", "build", "build_*", "out", ".venv"
)

Get-ChildItem -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue |
  Where-Object {
    if (-not $_.PSIsContainer) { return $false }
    foreach ($p in $junkPatterns) {
      if ($_.Name -like $p) { return $true }
    }
    return $false
  } |
  ForEach-Object {
    try { Remove-Item -LiteralPath $_.FullName -Recurse -Force } catch {}
  }

Get-ChildItem -LiteralPath $staging -Recurse -Force -File -ErrorAction SilentlyContinue |
  Where-Object {
    $name = $_.Name
    $name -like "*.pdb" -or $name -like "*.obj" -or $name -like "*.ilk" -or $name -like "*.idb" -or
    $name -like "*.exe" -or $name -like "*.dll"
  } |
  ForEach-Object {
    try { Remove-Item -LiteralPath $_.FullName -Force } catch {}
  }

$zipPath = Join-Path $root $ZipName
if (!( $zipPath.ToLower().EndsWith(".zip") )) {
  $zipPath = $zipPath + ".zip"
}

if (Test-Path -LiteralPath $zipPath) {
  Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zipPath -Force

Write-Host "Staging ready:  $staging"
Write-Host "Zip written:    $zipPath"
