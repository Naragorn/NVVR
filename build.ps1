param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$cmake = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
function Invoke-CleanEnvironment([string]$Program, [string[]]$Arguments) {
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Program
    $start.UseShellExecute = $false
    # The host supplies both Path and PATH. MSBuild's .NET Framework task
    # throws when building a case-insensitive dictionary from that block.
    # Normalize the child environment, without changing machine/user settings.
    $start.Environment.Clear()
    $names = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        if ($names.Add([string]$entry.Key)) { $start.Environment[[string]$entry.Key] = [string]$entry.Value }
    }
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "$Program failed with exit code $($process.ExitCode)" }
}
Invoke-CleanEnvironment $cmake @('-S', $PSScriptRoot, '-B', "$PSScriptRoot/build", '-A', 'Win32')
Invoke-CleanEnvironment $cmake @('--build', "$PSScriptRoot/build", '--config', $Configuration)
Invoke-CleanEnvironment $ctest @('--test-dir', "$PSScriptRoot/build", '-C', $Configuration, '--output-on-failure')
