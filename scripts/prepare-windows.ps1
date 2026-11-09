<#
.SYNOPSIS
Prepare lib-native's Windows archive overrides without Python or SQLite.
.DESCRIPTION
Run from an x64 Visual Studio developer PowerShell with the Windows SDK, installed
Silk and LLVM (clang, lld-link and llvm-lib or llvm-ar). SILK_CLANG or -Clang selects
LLVM. Downloads are pinned and SHA-256 checked against toolchains.lock.json.
Dawn is linked through a DLL so the DLL CRT initializes its C++ globals; Silk's
native executable entry point does not initialize the CRT. This also builds the
callback, ABI and benchmark archives and the Win32 bridge with SDK headers.
Only dawn, dawn-headers, glfw and win32-bridge overrides are updated; unrelated
configuration (including SQLite) is retained. No compiler or SDK is installed.
.EXAMPLE
.\scripts\prepare-windows.ps1
.EXAMPLE
silk build --manifest-path modules/gpu/webgpu/tests/native/silk.toml
.\scripts\prepare-windows.ps1 -RuntimeDirectory C:\path\to\executable-directory
Copies nonstd_dawn.dll, dxcompiler.dll, dxil.dll and glfw3.dll beside the executable.
The selected MSVC toolset's x64 VC runtime must also be installed on the run host.
#>
[CmdletBinding()]
param(
    [string] $Clang = $env:SILK_CLANG,
    [string] $RuntimeDirectory,
    [switch] $Json
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitProcess) {
    throw 'Preparation requires 64-bit Windows and an x64 Visual Studio developer shell.'
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or -not $env:INCLUDE -or -not $env:LIB) {
    throw 'Use an x64 Visual Studio developer shell with the C++ toolset and Windows SDK.'
}

$Repo = Split-Path -Parent $PSScriptRoot
$Webgpu = Join-Path $Repo 'modules/gpu/webgpu'
$Cache = Join-Path $Repo '.cache/windows'
$Lock = Get-Content -LiteralPath (Join-Path $Repo 'toolchains.lock.json') -Raw | ConvertFrom-Json
$Utf8 = New-Object System.Text.UTF8Encoding($false)

function Invoke-Native([string] $Program, [string[]] $Arguments) {
    # Keep compiler diagnostics out of -Json's machine-readable stdout.
    & $Program @Arguments | ForEach-Object { [Console]::Error.WriteLine($_) }
    if ($LASTEXITCODE -ne 0) { throw "$Program exited with status $LASTEXITCODE" }
}

function Resolve-Tool([string] $Name) {
    $Command = Get-Command $Name -CommandType Application -ErrorAction Stop | Select-Object -First 1
    return $Command.Source
}

if (-not $Clang) { $Clang = 'clang.exe' }
$Clang = Resolve-Tool $Clang
$LlvmBin = Split-Path -Parent $Clang
$Lld = Join-Path $LlvmBin 'lld-link.exe'
if (-not (Test-Path -LiteralPath $Lld -PathType Leaf)) { throw "Missing LLVM linker: $Lld" }
$Archiver = Join-Path $LlvmBin 'llvm-lib.exe'
$UseLlvmLib = Test-Path -LiteralPath $Archiver -PathType Leaf
if (-not $UseLlvmLib) { $Archiver = Join-Path $LlvmBin 'llvm-ar.exe' }
if (-not (Test-Path -LiteralPath $Archiver -PathType Leaf)) { throw "Missing LLVM archiver: $Archiver" }

function Find-ImportLibrary([string] $Name) {
    foreach ($Directory in ($env:LIB -split ';')) {
        if ($Directory) {
            $Candidate = Join-Path ($Directory.Trim('"')) "$Name.lib"
            if (Test-Path -LiteralPath $Candidate -PathType Leaf) { return $Candidate }
        }
    }
    throw "Cannot find $Name.lib in LIB; install the x64 C++ toolset and Windows SDK."
}

function Install-Asset([string] $Component, [string] $Required) {
    $Asset = $Lock.$Component.platforms.'x86_64-pc-windows-msvc'
    $Destination = Join-Path $Cache "$Component-$($Asset.sha256)"
    $Marker = Join-Path $Destination '.archive-sha256'
    if (Test-Path -LiteralPath $Destination) {
        if (-not (Test-Path -LiteralPath $Marker -PathType Leaf) -or
            [IO.File]::ReadAllText($Marker).Trim() -ne $Asset.sha256) {
            throw "Incomplete installation at $Destination; remove it and rerun preparation."
        }
    } else {
        $Downloads = Join-Path $Cache 'downloads'
        New-Item -ItemType Directory -Path $Downloads -Force | Out-Null
        $Name = [Uri]::UnescapeDataString(([Uri] $Asset.url).Segments[-1])
        $Archive = Join-Path $Downloads "$($Asset.sha256)-$Name"
        if (-not (Test-Path -LiteralPath $Archive -PathType Leaf)) {
            $Pending = "$Archive.$([Guid]::NewGuid().ToString('N')).partial"
            try {
                # Windows PowerShell 5.1 may otherwise negotiate obsolete TLS defaults.
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                Invoke-WebRequest -UseBasicParsing -Uri $Asset.url -OutFile $Pending
                if ((Get-FileHash -LiteralPath $Pending -Algorithm SHA256).Hash -ne $Asset.sha256) {
                    throw "SHA-256 mismatch for $($Asset.url)"
                }
                Move-Item -LiteralPath $Pending -Destination $Archive
            } finally {
                if (Test-Path -LiteralPath $Pending) { Remove-Item -LiteralPath $Pending -Force }
            }
        }
        if ((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $Asset.sha256) {
            throw "SHA-256 mismatch for cached $Archive; remove it and rerun preparation."
        }
        $Temporary = Join-Path $Cache ([Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $Temporary | Out-Null
        try {
            if ($Name.EndsWith('.zip')) {
                Expand-Archive -LiteralPath $Archive -DestinationPath $Temporary
            } else {
                Invoke-Native (Resolve-Tool 'tar.exe') @('-xf', $Archive, '-C', $Temporary)
            }
            $Tree = $Temporary
            if (-not $Asset.PSObject.Properties['strip_root'] -or $Asset.strip_root) {
                $Entries = @(Get-ChildItem -LiteralPath $Temporary -Force)
                if ($Entries.Count -ne 1 -or -not $Entries[0].PSIsContainer) {
                    throw "Expected one release directory in $Archive"
                }
                $Tree = $Entries[0].FullName
            }
            if (-not (Test-Path -LiteralPath (Join-Path $Tree $Required) -PathType Leaf)) {
                throw "Pinned $Component release lacks $Required"
            }
            [IO.File]::WriteAllText((Join-Path $Tree '.archive-sha256'), $Asset.sha256, $Utf8)
            Move-Item -LiteralPath $Tree -Destination $Destination
        } finally {
            if (Test-Path -LiteralPath $Temporary) { Remove-Item -LiteralPath $Temporary -Recurse -Force }
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Destination $Required) -PathType Leaf)) {
        throw "Pinned $Component installation lacks $Required at $Destination"
    }
    return $Destination
}

function New-StaticArchive([string] $Library, [string[]] $Objects) {
    if (Test-Path -LiteralPath $Library) { Remove-Item -LiteralPath $Library -Force }
    if ($UseLlvmLib) {
        Invoke-Native $Archiver (@("/out:$Library") + $Objects)
    } else {
        Invoke-Native $Archiver (@('rcs', $Library) + $Objects)
    }
}

$Imports = @{}
foreach ($Name in @('kernel32', 'ucrt', 'vcruntime')) { $Imports[$Name] = Find-ImportLibrary $Name }
$DawnSource = Install-Asset 'webgpu' 'lib/webgpu_dawn.lib'
$Glfw = Install-Asset 'glfw' 'lib-vc2022/glfw3dll.lib'
$Dxc = Join-Path (Install-Asset 'dxc' 'bin/x64/dxcompiler.dll') 'bin/x64'
$Dawn = Join-Path $Cache 'dawn'
$Bridge = Join-Path $Cache 'win32-bridge'
foreach ($Directory in @("$Dawn/lib", "$Dawn/bin", $Bridge)) {
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
}
# Refresh headers with the selected pin; do not mix two Dawn releases.
if (Test-Path -LiteralPath "$Dawn/include") { Remove-Item -LiteralPath "$Dawn/include" -Recurse -Force }
Copy-Item -LiteralPath "$DawnSource/include" -Destination "$Dawn/include" -Recurse
$Bindings = [IO.File]::ReadAllText((Join-Path $Webgpu 'sys/src/generated_webgpu.silk'))
$Symbols = @([regex]::Matches($Bindings, 'extern "C" fn (wgpu\w+)\(') |
    ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
if ($Symbols.Count -eq 0) { throw 'No Dawn C exports found in generated_webgpu.silk.' }
$Exports = Join-Path $Dawn 'exports.def'
[IO.File]::WriteAllText($Exports, "EXPORTS`n" + ($Symbols -join "`n") + "`n", $Utf8)
$Entry = Join-Path $Dawn 'entry.c'
[IO.File]::WriteAllText($Entry, "/* DLL CRT startup owns Dawn's C++ initialization. */`n", $Utf8)
Invoke-Native $Clang @('--target=x86_64-pc-windows-msvc', '-shared', "--ld-path=$Lld",
    '-fms-runtime-lib=dll', $Entry, "$DawnSource/lib/webgpu_dawn.lib", '-o', "$Dawn/bin/nonstd_dawn.dll",
    "-Wl,/def:$Exports", "-Wl,/implib:$Dawn/lib/nonstd_dawn.lib", '-luser32', '-lonecore_apiset', '-ldxguid')
foreach ($Name in @('ucrt', 'vcruntime')) {
    Copy-Item -LiteralPath $Imports[$Name] -Destination "$Dawn/lib/$Name.lib" -Force
}

$Support = @(
    @{ Name = 'nonstd_callbacks'; Source = 'sys/native/callbacks.c'; Flags = @('-O2') },
    @{ Name = 'nonstd_abi_layout'; Source = 'sys/abi/generated_layout.c'; Flags = @('-DSILK_GPU_ABI_LIBRARY', '-I', "$Dawn/include/dawn") },
    @{ Name = 'nonstd_bench_support'; Source = 'bench/support.c'; Flags = @('-O3', '-g', '-I', "$Webgpu/bench") }
)
foreach ($Item in $Support) {
    $Object = Join-Path $Dawn "$($Item.Name).obj"
    Invoke-Native $Clang (@('--target=x86_64-pc-windows-msvc', '-std=c11', '-fms-runtime-lib=dll',
        '-fms-omit-default-lib', '-I', "$Dawn/include") + $Item.Flags +
        @('-c', (Join-Path $Webgpu $Item.Source), '-o', $Object))
    New-StaticArchive "$Dawn/lib/$($Item.Name).lib" @($Object)
    Remove-Item -LiteralPath $Object
}

$Objects = @()
foreach ($Source in @('common/core.c', 'windows/bridge.c')) {
    $Object = Join-Path $Bridge (([IO.Path]::GetFileNameWithoutExtension($Source)) + '.obj')
    Invoke-Native $Clang @('--target=x86_64-pc-windows-msvc', '-fms-runtime-lib=dll', '-std=c11', '-O2',
        '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-missing-field-initializers',
        '-D_CRT_SECURE_NO_WARNINGS', '-c', (Join-Path $Repo "modules/platform/window/native/$Source"), '-o', $Object)
    $Objects += $Object
}
New-StaticArchive "$Bridge/silkwindow.lib" $Objects
foreach ($Object in $Objects) { Remove-Item -LiteralPath $Object }
foreach ($Name in @('kernel32', 'ucrt', 'vcruntime')) {
    Copy-Item -LiteralPath $Imports[$Name] -Destination "$Bridge/$Name.lib" -Force
}
Copy-Item -LiteralPath $Imports['kernel32'] -Destination "$Glfw/lib-vc2022/kernel32.lib" -Force

# Silk treats paths not beginning with / as relative to SILK_HOME, including C:/...
function ConvertTo-SilkPath([string] $Path) {
    $Absolute = [IO.Path]::GetFullPath($Path).Replace('\', '/')
    if ($Absolute.StartsWith('//')) { throw 'Silk native overrides require a local drive path, not a UNC path.' }
    return '/' + $Absolute
}

function Set-Override([string] $Text, [string] $Name, [string] $Path) {
    $Encoded = ConvertTo-SilkPath $Path | ConvertTo-Json -Compress
    # Accept bare, double-quoted or literal-quoted names, leaving every unrelated
    # section and every other key intact. Only this override's path is managed.
    $Parts = @('native', 'override', $Name) | ForEach-Object {
        $Escaped = [regex]::Escape($_)
        '(?:' + $Escaped + '|"' + $Escaped + '"|\x27' + $Escaped + '\x27)'
    }
    $Header = '(?m)^[\t ]*\[[\t ]*' + ($Parts -join '[\t ]*\.[\t ]*') + '[\t ]*\][\t ]*(?:#[^\r\n]*)?\r?$'
    $Match = [regex]::Match($Text, $Header)
    if (-not $Match.Success) {
        return $Text.TrimEnd("`r", "`n") + "`n`n[native.override.$Name]`npath = $Encoded`n"
    }
    $Start = $Match.Index + $Match.Length
    $Next = [regex]::Match($Text.Substring($Start), '(?m)^[\t ]*\[')
    $End = if ($Next.Success) { $Start + $Next.Index } else { $Text.Length }
    $Body = $Text.Substring($Start, $End - $Start)
    $PathLine = [regex]::new('(?m)^[\t ]*(?:path|"path"|\x27path\x27)[\t ]*=[^\r\n]*')
    if ($PathLine.IsMatch($Body)) {
        $Replacement = "path = $Encoded"
        $Body = $PathLine.Replace($Body, [System.Text.RegularExpressions.MatchEvaluator] { param($m) $Replacement })
    } else {
        $Body = "`npath = $Encoded" + $Body
        if (-not $Body.EndsWith("`n")) { $Body += "`n" }
    }
    return $Text.Substring(0, $Start) + $Body + $Text.Substring($End)
}

$SilkHome = if ($env:SILK_HOME) { [IO.Path]::GetFullPath($env:SILK_HOME) } else { Join-Path $HOME '.silk' }
New-Item -ItemType Directory -Path $SilkHome -Force | Out-Null
$Overrides = Join-Path $SilkHome 'native-overrides.toml'
$Text = if (Test-Path -LiteralPath $Overrides) { [IO.File]::ReadAllText($Overrides) } else { '' }
foreach ($Item in @(
    @{ Name = 'dawn'; Path = $Dawn }, @{ Name = 'dawn-headers'; Path = $Dawn },
    @{ Name = 'glfw'; Path = $Glfw }, @{ Name = 'win32-bridge'; Path = $Bridge }
)) { $Text = Set-Override $Text $Item.Name $Item.Path }
$PendingOverrides = "$Overrides.$([Guid]::NewGuid().ToString('N')).tmp"
try {
    [IO.File]::WriteAllText($PendingOverrides, $Text, $Utf8)
    if (Test-Path -LiteralPath $Overrides) {
        # PowerShell converts $null to an empty string for this .NET parameter.
        [IO.File]::Replace($PendingOverrides, $Overrides, [NullString]::Value)
    } else {
        [IO.File]::Move($PendingOverrides, $Overrides)
    }
} finally {
    if (Test-Path -LiteralPath $PendingOverrides) { Remove-Item -LiteralPath $PendingOverrides -Force }
}

if ($RuntimeDirectory) {
    New-Item -ItemType Directory -Path $RuntimeDirectory -Force | Out-Null
    foreach ($Dll in @("$Dawn/bin/nonstd_dawn.dll", "$Dxc/dxcompiler.dll", "$Dxc/dxil.dll", "$Glfw/lib-vc2022/glfw3.dll")) {
        Copy-Item -LiteralPath $Dll -Destination $RuntimeDirectory -Force
    }
}
$Result = [ordered] @{ dawn = $Dawn; glfw = $Glfw; dxc = $Dxc; bridge = $Bridge; overrides = $Overrides }
if ($Json) { $Result | ConvertTo-Json -Compress } else { Write-Host "Prepared native archives; updated $Overrides" }
