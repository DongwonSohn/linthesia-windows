[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [Parameter(Mandatory = $true)]
    [string]$PortableCorePath,
    [Parameter(Mandatory = $true)]
    [string]$ObjdumpPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).Path
$PortableCorePath = (Resolve-Path -LiteralPath $PortableCorePath).Path
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
Assert-Check ($rootEntries.Count -eq 2) 'Package root must contain only Linthesia.exe and app/.'
$launcher = Join-Path $PackageRoot 'Linthesia.exe'
$app = Join-Path $PackageRoot 'app'
Assert-Check (Test-Path -LiteralPath $launcher -PathType Leaf) 'Root launcher is missing.'
Assert-Check (Test-Path -LiteralPath $app -PathType Container) 'Application folder is missing.'
Assert-Check (Test-Path -LiteralPath (Join-Path $app 'Linthesia.exe') -PathType Leaf) 'Internal executable is missing.'
Assert-Check (@(Get-ChildItem -LiteralPath $PackageRoot -Filter '*.dll' -File).Count -eq 0) 'DLLs must stay inside app/.'
Assert-Check (Test-Path -LiteralPath (Join-Path $app 'graphics/linthesia.png') -PathType Leaf) 'Packaged graphics are missing.'
$defaultMidi = Join-Path $app 'music/Default/Eve ／ 夜は仄か.mid'
Assert-Check (Test-Path -LiteralPath $defaultMidi -PathType Leaf) 'The default MIDI song is missing.'

# The launcher must work before app/ has been added to any DLL search path.
$launcherImports = @(Get-DllImports $launcher)
Assert-Check ($launcherImports.Count -gt 0) 'Cannot find launcher PE imports.'
foreach ($dll in $launcherImports) {
    Assert-Check (Test-SystemDll $dll) "Launcher depends on a non-system DLL: $dll"
}

# All non-system imports of the actual application and every bundled DLL must
# resolve beside the internal executable, without relying on the developer PATH.
$binaries = @(Get-ChildItem -LiteralPath $app -File | Where-Object {
    $_.Extension -in @('.exe', '.dll')
})
foreach ($binary in $binaries) {
    foreach ($dll in @(Get-DllImports $binary.FullName)) {
        Assert-Check ($dll -notmatch '^(msys-|cygwin)') "Unexpected MSYS/Cygwin dependency: $dll"
        Assert-Check ((Test-SystemDll $dll) -or
            (Test-Path -LiteralPath (Join-Path $app $dll) -PathType Leaf)) `
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
            # Kill the wrapper and its child so a broken launch cannot stall CI.
            & "$env:SystemRoot\System32\taskkill.exe" /PID $process.Id /T /F | Out-Null
            throw "Launcher timed out for arguments: $Arguments"
        }
        $process.Refresh()
        if ($process.ExitCode -ne $ExpectedExit) {
            $output = (Get-Content -LiteralPath $stdout -Raw) + (Get-Content -LiteralPath $stderr -Raw)
            throw "Launcher returned $($process.ExitCode), expected $ExpectedExit for $Arguments. $output"
        }
    }

    Invoke-Launcher '--version' 0
    Invoke-Launcher '--help' 0
    Invoke-Launcher '-f' 1

    # Replace only this temporary copy's child with the existing parser test.
    # It reads each argv filename from the inherited working directory, proving
    # that the launcher preserves Unicode, spaces, shell metacharacters, and CWD.
    Copy-Item -LiteralPath $PortableCorePath -Destination (Join-Path $relocated 'app/Linthesia.exe') -Force
    $firstMidi = '연주 파일.mid'
    $secondMidi = "곡 & 100% (Ω)'s.mid"
    foreach ($name in @($firstMidi, $secondMidi)) {
        Copy-Item -LiteralPath $defaultMidi -Destination (Join-Path $workingDirectory $name)
    }
    Invoke-Launcher ('"{0}" "{1}"' -f $firstMidi, $secondMidi) 0
    Invoke-Launcher 'missing-song.mid' 1
    Write-Host 'Compact package, dependency confinement, Unicode arguments, working directory, and exit codes passed.'
} finally {
    $env:PATH = $oldPath
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
