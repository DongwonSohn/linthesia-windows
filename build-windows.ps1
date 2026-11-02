[CmdletBinding()]
param(
    [string]$MsysRoot = 'C:\msys64',
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sourceRoot = $PSScriptRoot
$ucrtRoot = Join-Path $MsysRoot 'ucrt64'
$toolBin = Join-Path $ucrtRoot 'bin'
$buildRoot = Join-Path $sourceRoot 'build-windows'
$distRoot = Join-Path $sourceRoot 'dist\windows-x64'
$stageRoot = Join-Path $distRoot 'Linthesia'
$zipPath = Join-Path $distRoot 'Linthesia-windows-x64.zip'
$cmake = Join-Path $toolBin 'cmake.exe'
$objdump = Join-Path $toolBin 'objdump.exe'

foreach ($tool in @('cmake.exe', 'ctest.exe', 'ninja.exe', 'g++.exe', 'windres.exe', 'objdump.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $toolBin $tool))) {
        throw "Missing UCRT64 tool $tool. Install the packages in WINDOWS-BUILD.ko.md first, or pass -MsysRoot with your MSYS2 installation path."
    }
}

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed (exit $LASTEXITCODE)."
    }
}

$oldPath = $env:PATH
try {
    $env:PATH = "$toolBin;$oldPath"
    Invoke-Checked $cmake @('-S', $sourceRoot, '-B', $buildRoot, '-G', 'Ninja',
        "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_PREFIX_PATH=$ucrtRoot",
        "-DCMAKE_CXX_COMPILER=$(Join-Path $toolBin 'g++.exe')",
        "-DCMAKE_RC_COMPILER=$(Join-Path $toolBin 'windres.exe')",
        "-DCMAKE_MAKE_PROGRAM=$(Join-Path $toolBin 'ninja.exe')")
    Invoke-Checked $cmake @('--build', $buildRoot, '--parallel')
    Invoke-Checked (Join-Path $toolBin 'ctest.exe') @('--test-dir', $buildRoot, '--output-on-failure')

    # Rebuild only the generated distribution folder so stale DLLs cannot hide
    # missing dependencies after a toolchain update.
    if (Test-Path -LiteralPath $stageRoot) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
    Invoke-Checked $cmake @('--install', $buildRoot, '--prefix', $stageRoot)

    # Follow PE imports recursively, including the compiler runtime. Windows
    # system DLLs come from the OS; no MSYS2 installation is needed to run.
    $pending = New-Object 'System.Collections.Generic.Queue[string]'
    $visited = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $pending.Enqueue((Join-Path $stageRoot 'Linthesia.exe'))
    # SDL_image may load PNG support dynamically in some package versions.
    foreach ($extra in @('libpng16-16.dll', 'zlib1.dll')) {
        $extraPath = Join-Path $toolBin $extra
        if (Test-Path -LiteralPath $extraPath) {
            Copy-Item -LiteralPath $extraPath -Destination $stageRoot -Force
            $pending.Enqueue((Join-Path $stageRoot $extra))
        }
    }
    while ($pending.Count -gt 0) {
        $binary = $pending.Dequeue()
        if (-not $visited.Add($binary)) { continue }
        $imports = & $objdump '-p' $binary
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE imports: $binary" }
        foreach ($line in $imports) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $dllName = $Matches[1]
            if ($dllName -match '^(msys-|cygwin)') {
                throw "Unexpected MSYS/Cygwin runtime dependency: $dllName"
            }
            if ($dllName -match '^(api-ms-|ext-ms-)' -or
                (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$dllName"))) {
                continue
            }
            $destination = Join-Path $stageRoot $dllName
            if (-not (Test-Path -LiteralPath $destination)) {
                $dependency = Join-Path $toolBin $dllName
                if (-not (Test-Path -LiteralPath $dependency)) {
                    throw "Unresolved runtime dependency $dllName (required by $binary)."
                }
                Copy-Item -LiteralPath $dependency -Destination $destination
            }
            $pending.Enqueue($destination)
        }
    }

    $licenseRoot = Join-Path $ucrtRoot 'share\licenses'
    if (Test-Path -LiteralPath $licenseRoot) {
        Copy-Item -LiteralPath $licenseRoot -Destination (Join-Path $stageRoot 'licenses') -Recurse
    }
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -LiteralPath $stageRoot -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Executable: $(Join-Path $stageRoot 'Linthesia.exe')"
    Write-Host "Portable ZIP: $zipPath"
} finally {
    $env:PATH = $oldPath
}
