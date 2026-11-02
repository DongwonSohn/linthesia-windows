[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [Parameter(Mandatory = $true)]
    [string]$LauncherProbePath,
    [Parameter(Mandatory = $true)]
    [string]$ObjdumpPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$LauncherProbePath = (Resolve-Path -LiteralPath $LauncherProbePath).Path
$ObjdumpPath = (Resolve-Path -LiteralPath $ObjdumpPath).Path

function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-DllImports([string]$Binary) {
    $headers = & $ObjdumpPath '-p' $Binary
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE imports: $Binary" }
    foreach ($line in $headers) {
        if ($line -match 'DLL Name:\s*(\S+)') { $Matches[1] }
    }
}

function Test-SystemDll([string]$Name) {
    return $Name -match '^(api-ms-|ext-ms-)' -or
        (Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32\$Name"))
}

$rootEntries = @(Get-ChildItem -LiteralPath $PackageRoot -Force)
$expectedRootNames = @('Linthesia.exe', 'dll', 'graphics', 'music', 'licenses', 'manual')
Assert-Check ((($rootEntries.Name | Sort-Object) -join '|') -eq
    (($expectedRootNames | Sort-Object) -join '|')) `
    'Package root must contain Linthesia.exe, dll/, graphics/, music/, licenses/, and manual/.'
$launcher = Join-Path $PackageRoot 'Linthesia.exe'
$dllRoot = Join-Path $PackageRoot 'dll'
Assert-Check (Test-Path -LiteralPath $launcher -PathType Leaf) 'Root launcher is missing.'
foreach ($folder in @('dll', 'graphics', 'music', 'licenses', 'manual')) {
    Assert-Check (Test-Path -LiteralPath (Join-Path $PackageRoot $folder) -PathType Container) `
        "Required package folder is missing: $folder"
}
Assert-Check (Test-Path -LiteralPath (Join-Path $dllRoot 'LinthesiaApp.dll') -PathType Leaf) 'Application DLL is missing.'
Assert-Check (@(Get-ChildItem -LiteralPath $PackageRoot -Filter '*.dll' -File).Count -eq 0) 'DLLs must stay inside dll/.'
$dllEntries = @(Get-ChildItem -LiteralPath $dllRoot -Force)
Assert-Check ($dllEntries.Count -gt 1) 'Bundled application dependencies are missing.'
foreach ($entry in $dllEntries) {
    Assert-Check ((-not $entry.PSIsContainer) -and $entry.Extension -ieq '.dll') `
        "The dll folder must contain only DLL files: $($entry.Name)"
}
foreach ($dll in @(Get-ChildItem -LiteralPath $PackageRoot -Filter '*.dll' -File -Recurse)) {
    Assert-Check ($dll.DirectoryName -ieq $dllRoot) "DLL is outside dll/: $($dll.FullName)"
}
foreach ($document in @('README-ko.md', 'COPYING')) {
    Assert-Check (Test-Path -LiteralPath (Join-Path $PackageRoot "manual/$document") -PathType Leaf) `
        "Required manual document is missing: $document"
}
Assert-Check (Test-Path -LiteralPath (Join-Path $PackageRoot 'graphics/linthesia.png') -PathType Leaf) 'Packaged graphics are missing.'
$defaultMidi = Join-Path $PackageRoot 'music/Default/Eve ／ 夜は仄か.mid'
Assert-Check (Test-Path -LiteralPath $defaultMidi -PathType Leaf) 'The default MIDI song is missing.'

# The launcher must run before it can load the application from dll/.
$launcherImports = @(Get-DllImports $launcher)
Assert-Check ($launcherImports.Count -gt 0) 'Cannot find launcher PE imports.'
foreach ($dll in $launcherImports) {
    Assert-Check (Test-SystemDll $dll) "Launcher depends on a non-system DLL: $dll"
}

# All non-system imports of the application DLL and every bundled DLL must
# resolve within dll/, without relying on the developer PATH.
$binaries = @(Get-ChildItem -LiteralPath $dllRoot -Filter '*.dll' -File)
foreach ($binary in $binaries) {
    foreach ($dll in @(Get-DllImports $binary.FullName)) {
        Assert-Check ($dll -notmatch '^(msys-|cygwin)') "Unexpected MSYS/Cygwin dependency: $dll"
        Assert-Check ((Test-SystemDll $dll) -or
            (Test-Path -LiteralPath (Join-Path $dllRoot $dll) -PathType Leaf)) `
            "Missing dependency $dll required by $($binary.Name)."
    }
}

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ('linthesia-package-' + [Guid]::NewGuid().ToString('N'))
$oldPath = $env:PATH
$script:processRun = 0
try {
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null
    $relocated = Join-Path $temporaryRoot 'Linthesia 테스트 Ω'
    Copy-Item -LiteralPath $PackageRoot -Destination $relocated -Recurse
    $workingDirectory = Join-Path $temporaryRoot '작업 폴더'
    New-Item -ItemType Directory -Path $workingDirectory | Out-Null
    $relocatedLauncher = Join-Path $relocated 'Linthesia.exe'
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"

    function Invoke-Launcher([string]$Arguments, [int]$ExpectedExit) {
        $script:processRun++
        $stdout = Join-Path $temporaryRoot "stdout-$script:processRun.txt"
        $stderr = Join-Path $temporaryRoot "stderr-$script:processRun.txt"
        $process = Start-Process -FilePath $relocatedLauncher -ArgumentList $Arguments `
            -WorkingDirectory $workingDirectory -RedirectStandardOutput $stdout `
            -RedirectStandardError $stderr -PassThru
        if (-not $process.WaitForExit(30000)) {
            # A loader failure or accidental GUI launch must not stall CI.
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F | Out-Null
            throw "Launcher timed out for arguments: $Arguments"
        }
        $process.Refresh()
        $standardOutput = [string](Get-Content -LiteralPath $stdout -Raw -Encoding utf8)
        $standardError = [string](Get-Content -LiteralPath $stderr -Raw -Encoding utf8)
        if ($process.ExitCode -ne $ExpectedExit) {
            throw "Launcher returned $($process.ExitCode), expected $ExpectedExit for $Arguments. $standardOutput $standardError"
        }
        return [pscustomobject]@{ Stdout = $standardOutput; Stderr = $standardError }
    }

    $version = Invoke-Launcher '--version' 0
    Assert-Check ($version.Stdout.Contains('Linthesia')) 'Version output was not forwarded.'
    $help = Invoke-Launcher '--help' 0
    Assert-Check ($help.Stdout.Contains('Options:')) 'Help output was not forwarded.'
    $expectedMusicPath = $relocated.Replace('\', '/') + '/music'
    Assert-Check ($help.Stdout.Replace('\', '/').Contains($expectedMusicPath)) `
        'Application resources must resolve from the package root, not dll/.'
    $invalid = Invoke-Launcher '-f' 1
    Assert-Check ($invalid.Stdout.Contains('Missing value for option:')) 'Invalid option output was not forwarded.'

    # Replace only this temporary copy's application DLL with the parser probe.
    # It requires both filenames and reads them from the inherited working
    # directory, proving Unicode argument forwarding and CWD preservation.
    Copy-Item -LiteralPath $LauncherProbePath -Destination (Join-Path $relocated 'dll/LinthesiaApp.dll') -Force
    $firstMidi = '연주 파일.mid'
    $secondMidi = "곡 & 100% (Ω)'s.mid"
    foreach ($name in @($firstMidi, $secondMidi)) {
        Copy-Item -LiteralPath $defaultMidi -Destination (Join-Path $workingDirectory $name)
    }
    $probe = Invoke-Launcher ('"{0}" "{1}"' -f $firstMidi, $secondMidi) 0
    Assert-Check ($probe.Stdout.Contains('Portable core checks passed')) 'Parser probe output was not forwarded.'
    $missing = Invoke-Launcher ('"missing-song.mid" "{0}"' -f $secondMidi) 1
    Assert-Check (-not [string]::IsNullOrWhiteSpace($missing.Stderr)) 'Missing MIDI error output was not forwarded.'
    Write-Host 'DLL-only package layout, dependency confinement, resource paths, Unicode arguments, working directory, output, and exit codes passed.'
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
