param(
  [string]$MapsDir = "maps",
  [string]$AgentsConfig = "configs/large_agents.txt"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function To-IntScalar {
  param([Parameter(Mandatory = $true)] $Value)
  if ($Value -is [System.Array]) {
    if ($Value.Count -eq 0) { return 0 }
    return [int]$Value[0]
  }
  return [int]$Value
}

function Key {
  param([int]$x, [int]$y, [int]$w)
  return $y * $w + $x
}

function Get-NeighborOffsets {
  return @(
    @([int]1, [int]0),
    @([int]-1, [int]0),
    @([int]0, [int]1),
    @([int]0, [int]-1)
  )
}

function ReachableCount {
  param(
    [Parameter(Mandatory = $true)] [object[]]$Grid,
    [int]$Width,
    [int]$Height,
    [int]$StartX,
    [int]$StartY
  )

  if ($StartX -lt 0 -or $StartX -ge $Width -or $StartY -lt 0 -or $StartY -ge $Height) { return 0 }
  if ([char]$Grid[$StartY][$StartX] -ne '.') { return 0 }

  $seen = New-Object 'System.Collections.Generic.HashSet[int]'
  $q = New-Object 'System.Collections.Generic.Queue[object]'
  $q.Enqueue(@([int]$StartX, [int]$StartY))
  [void]$seen.Add((Key -x $StartX -y $StartY -w $Width))

  $dirs = Get-NeighborOffsets
  while ($q.Count -gt 0) {
    $pt = $q.Dequeue()
    $x = To-IntScalar $pt[0]
    $y = To-IntScalar $pt[1]

    foreach ($d in $dirs) {
      $nx = $x + (To-IntScalar $d[0])
      $ny = $y + (To-IntScalar $d[1])
      if ($nx -lt 0 -or $nx -ge $Width -or $ny -lt 0 -or $ny -ge $Height) { continue }
      if ([char]$Grid[$ny][$nx] -ne '.') { continue }

      $k = Key -x $nx -y $ny -w $Width
      if ($seen.Add($k)) {
        $q.Enqueue(@([int]$nx, [int]$ny))
      }
    }
  }

  return $seen.Count
}

function NearestFree {
  param(
    [Parameter(Mandatory = $true)] [object[]]$Grid,
    [int]$Width,
    [int]$Height,
    [int]$StartX,
    [int]$StartY
  )

  $Width = To-IntScalar $Width
  $Height = To-IntScalar $Height
  $StartX = To-IntScalar $StartX
  $StartY = To-IntScalar $StartY

  $sx = [int][Math]::Max(0, [Math]::Min(($Width - 1), $StartX))
  $sy = [int][Math]::Max(0, [Math]::Min(($Height - 1), $StartY))
  if ([char]$Grid[$sy][$sx] -eq '.') { return @([int]$sx, [int]$sy) }

  for ($y = 0; $y -lt $Height; $y++) {
    for ($x = 0; $x -lt $Width; $x++) {
      if ([char]$Grid[$y][$x] -eq '.') { return @([int]$x, [int]$y) }
    }
  }

  return @([int]0, [int]0)
}

$agents = @()
Get-Content -LiteralPath $AgentsConfig | ForEach-Object {
  $line = ([string]$_).Trim()
  if (-not $line) { return }
  $parts = $line -split '\s+'
  if ($parts.Count -lt 5) { return }

  $agents += [pscustomobject]@{
    id = [int]$parts[0]
    sx = [int]$parts[1]
    sy = [int]$parts[2]
    gx = [int]$parts[3]
    gy = [int]$parts[4]
  }
}

$report = @()
$maps = Get-ChildItem -LiteralPath $MapsDir -Filter '*.json' | Sort-Object Name
foreach ($mapFile in $maps) {
  $name = $mapFile.Name
  $json = Get-Content -LiteralPath $mapFile.FullName -Raw | ConvertFrom-Json

  $w = [int]$json.width
  $h = [int]$json.height
  $rows = @($json.grid)

  $okDims = ($rows.Count -eq $h)
  $badLen = 0
  $badChar = 0
  $free = 0

  $grid = @()
  foreach ($rowObj in $rows) {
    $line = [string]$rowObj
    if ($line.Length -ne $w) { $badLen++ }

    $srcChars = [char[]]$line
    $chars = New-Object 'char[]' $w
    for ($i = 0; $i -lt $w; $i++) { $chars[$i] = '#' }
    $copyLen = [Math]::Min($w, $srcChars.Length)
    for ($i = 0; $i -lt $copyLen; $i++) { $chars[$i] = $srcChars[$i] }

    foreach ($ch in $chars) {
      if ($ch -eq '.') { $free++ }
      elseif ($ch -ne '#') { $badChar++ }
    }

    $grid += ,$chars
  }

  $components = 0
  $largest = 0
  $seen = New-Object 'System.Collections.Generic.HashSet[int]'
  $dirs = Get-NeighborOffsets

  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      if ([char]$grid[$y][$x] -ne '.') { continue }

      $k = Key -x $x -y $y -w $w
      if ($seen.Contains($k)) { continue }

      $components++
      $cnt = ReachableCount -Grid $grid -Width $w -Height $h -StartX $x -StartY $y
      if ($cnt -gt $largest) { $largest = $cnt }

      $q = New-Object 'System.Collections.Generic.Queue[object]'
      $q.Enqueue(@([int]$x, [int]$y))
      [void]$seen.Add($k)

      while ($q.Count -gt 0) {
        $pt = $q.Dequeue()
        $px = To-IntScalar $pt[0]
        $py = To-IntScalar $pt[1]

        foreach ($d in $dirs) {
          $nx = $px + (To-IntScalar $d[0])
          $ny = $py + (To-IntScalar $d[1])
          if ($nx -lt 0 -or $nx -ge $w -or $ny -lt 0 -or $ny -ge $h) { continue }
          if ([char]$grid[$ny][$nx] -ne '.') { continue }

          $nk = Key -x $nx -y $ny -w $w
          if ($seen.Add($nk)) {
            $q.Enqueue(@([int]$nx, [int]$ny))
          }
        }
      }
    }
  }

  $agentReach = @()
  foreach ($a in $agents) {
    $s = NearestFree -Grid $grid -Width $w -Height $h -StartX ([int]$a.sx) -StartY ([int]$a.sy)
    $sx = To-IntScalar $s[0]
    $sy = To-IntScalar $s[1]
    $rc = ReachableCount -Grid $grid -Width $w -Height $h -StartX $sx -StartY $sy
    $agentReach += [int]$rc
  }

  $minAgent = 0
  if ($agentReach.Count -gt 0) {
    $minAgent = [int](($agentReach | Measure-Object -Minimum).Minimum)
  }

  $layoutOk = $okDims -and ($badLen -eq 0) -and ($badChar -eq 0)
  $playable = $layoutOk -and ($free -ge 80) -and ($largest -ge 80) -and ($minAgent -ge 40)

  $report += [pscustomobject]@{
    Map = $name
    W = $w
    H = $h
    Free = $free
    Components = $components
    LargestComp = $largest
    MinAgentReach = $minAgent
    LayoutOK = $layoutOk
    Playable = $playable
  }
}

$report | Format-Table -AutoSize
Write-Host ""
Write-Host "Playable maps:"
$report | Where-Object { $_.Playable } | Select-Object -ExpandProperty Map
