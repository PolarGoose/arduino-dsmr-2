Function Info($msg) {
  Write-Host -ForegroundColor DarkGreen "`nINFO: $msg`n"
}

Function Error($msg) {
  Write-Host `n`n
  Write-Error $msg
  exit 1
}

Function CheckReturnCodeOfPreviousCommand($msg) {
  if(-Not $?) {
    Error "${msg}. Error code: $LastExitCode"
  }
}

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$root = Resolve-Path $PSScriptRoot
$buildDir = "$root/build"

Info "Find Visual Studio installation path"
$vswhereCommand = Get-Command -Name "${Env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installationPath = & $vswhereCommand -prerelease -latest -property installationPath

Info "Open Visual Studio 2022 Developer PowerShell"
& "$installationPath\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64

Info "Build and test debug version"
Info "Cmake generate cache"
cmake `
  -S $root `
  -B $buildDir/win-debug `
  -G Ninja `
  -D CMAKE_BUILD_TYPE=Debug
CheckReturnCodeOfPreviousCommand "cmake cache failed"

Info "Cmake build"
cmake --build $buildDir/win-debug
CheckReturnCodeOfPreviousCommand "cmake build failed"

Info "Run tests"
& "$buildDir/win-debug/arduino_dsmr_test.exe"
CheckReturnCodeOfPreviousCommand "tests failed"

Info "Build and test release version"
Info "Cmake generate cache"
cmake `
  -S $root `
  -B $buildDir/win-release `
  -G Ninja `
  -D CMAKE_BUILD_TYPE=Release
CheckReturnCodeOfPreviousCommand "cmake cache failed"

Info "Cmake build"
cmake --build $buildDir/win-release
CheckReturnCodeOfPreviousCommand "cmake build failed"

Info "Run tests"
& "$buildDir/win-release/arduino_dsmr_test.exe"
CheckReturnCodeOfPreviousCommand "tests failed"
