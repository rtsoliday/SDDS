[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [ValidateSet("build", "clean", "rebuild", "test")]
  [string]$Target = "build",

  [Parameter(Position = 1)]
  [ValidateRange(1, 1024)]
  [int]$Jobs = [Environment]::ProcessorCount
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-File {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [Parameter(Mandatory = $true)]
    [string]$Description
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    throw "$Description was not found at '$Path'."
  }

  return (Resolve-Path -LiteralPath $Path).Path
}

function Require-Directory {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Path,

    [Parameter(Mandatory = $true)]
    [string]$Description
  )

  if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
    throw "$Description was not found at '$Path'."
  }

  return (Resolve-Path -LiteralPath $Path).Path
}

function Find-VisualStudio {
  if ($env:VSINSTALLDIR) {
    $vcvars = Join-Path $env:VSINSTALLDIR "VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path -LiteralPath $vcvars -PathType Leaf) {
      return (Resolve-Path -LiteralPath $vcvars).Path
    }
  }

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  $vswhere = Require-File $vswhere "Visual Studio Installer's vswhere.exe"
  $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($LASTEXITCODE -ne 0 -or -not $installationPath) {
    throw "Visual Studio with the x64 C++ build tools was not found."
  }

  $vcvars = Join-Path ($installationPath | Select-Object -First 1) "VC\Auxiliary\Build\vcvars64.bat"
  return Require-File $vcvars "Visual Studio's vcvars64.bat"
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$vcvars64 = Find-VisualStudio

$cygwinRoot = if ($env:SDDS_CYGWIN_ROOT) { $env:SDDS_CYGWIN_ROOT } else { "C:\cygwin64" }
$cygwinRoot = Require-Directory $cygwinRoot "Cygwin"
$cygwinBin = Require-Directory (Join-Path $cygwinRoot "bin") "Cygwin bin directory"
$requiredCygwinTools = @("sh.exe", "uname.exe", "cp.exe", "rm.exe", "mkdir.exe")
foreach ($tool in $requiredCygwinTools) {
  $null = Require-File (Join-Path $cygwinBin $tool) "Cygwin tool $tool"
}
$shell = Require-File (Join-Path $cygwinBin "sh.exe") "Cygwin shell"

if ($env:SDDS_MAKE_EXE) {
  $make = Require-File $env:SDDS_MAKE_EXE "GNU Make"
} else {
  $makeCandidates = @(
    (Join-Path $cygwinBin "make.exe"),
    "C:\Strawberry\c\bin\make.exe"
  )
  $make = $makeCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
  if (-not $make) {
    throw "GNU Make was not found. Install the Cygwin make package or Strawberry Perl, or set SDDS_MAKE_EXE."
  }
  $make = (Resolve-Path -LiteralPath $make).Path
}
$makeBin = Split-Path -Parent $make

$qtRoot = "C:\Qt\6.8.2\msvc2022_64"
$qtRoot = Require-Directory $qtRoot "Qt 6.8.2 for MSVC 2022"
$qtBin = Require-Directory (Join-Path $qtRoot "bin") "Qt bin directory"
$null = Require-File (Join-Path $qtBin "moc.exe") "Qt moc.exe"

$gslRoot = Require-Directory (Join-Path $repositoryRoot "..\gsl") "Sibling GSL source tree"
$hdf5Root = Require-Directory (Join-Path $repositoryRoot "..\hdf5\HDF5-1.14.6-win64") "HDF5 1.14.6 Windows distribution"

Write-Host "SDDS Windows build environment"
Write-Host "  Visual Studio: $vcvars64"
Write-Host "  Cygwin:        $cygwinRoot"
Write-Host "  GNU Make:      $make"
Write-Host "  Qt:            $qtRoot"
Write-Host "  GSL:           $gslRoot"
Write-Host "  HDF5:          $hdf5Root"
Write-Host "  Target:        $Target"
if ($Target -ne "clean") {
  Write-Host "  Jobs:          $Jobs"
}

$shellForMake = $shell.Replace("\", "/")
$commonArguments = "OS=Windows ARCH=x86_64 SHELL=$shellForMake"
$quotedMake = '"{0}"' -f $make
$quotedRoot = '"{0}"' -f $repositoryRoot
$makeBuild = "$quotedMake -C $quotedRoot -j$Jobs $commonArguments"
$makeClean = "$quotedMake -C $quotedRoot $commonArguments clean"

switch ($Target) {
  "build" { $makeCommands = @($makeBuild) }
  "clean" { $makeCommands = @($makeClean) }
  "rebuild" { $makeCommands = @($makeClean, $makeBuild) }
  "test" { $makeCommands = @("$makeBuild test") }
}

$pathCommand = 'set "PATH={0};{1};{2};%PATH%"' -f $qtBin, $cygwinBin, $makeBin
$command = @(
  $pathCommand,
  ('call "{0}"' -f $vcvars64),
  ($makeCommands -join " && ")
) -join " && "

& $env:ComSpec /d /s /c $command
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
  [Console]::Error.WriteLine("SDDS Windows $Target failed with exit code $exitCode.")
}

exit $exitCode
