# installation for windows env
# https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.ps1 | iex

param(
    [string]$Version = "",
    [string]$InstallDir = "",
    [switch]$NoModifyPath,
    [switch]$Force,
    [switch]$Help,
    [switch]$Uninstall
)

$script:AppName      = "sniffercommit"
$script:RepoOwner   = "slowy07"
$script:RepoName    = "sniffercommit"
$script:RepoUrl     = "https://github.com/$RepoOwner/$RepoName"
$script:ReleaseApi  = "https://api.github.com/repos/$RepoOwner/$RepoName/releases"
$script:Branch      = if ($env:SNIFFERCOMMIT_BRANCH) { $env:SNIFFERCOMMIT_BRANCH } else { "develop" }

$script:TargetVersion = if ($Version) { $Version } else { if ($env:SNIFFERCOMMIT_VERSION) { $env:SNIFFERCOMMIT_VERSION } else { "latest" } }
$script:TargetDir     = if ($InstallDir) { $InstallDir } else { if ($env:SNIFFERCOMMIT_INSTALL_DIR) { $env:SNIFFERCOMMIT_INSTALL_DIR } else { "" } }
$script:NoColor       = if ($env:NO_COLOR) { [bool][int]$env:NO_COLOR } else { $false }
$script:ForceBuild    = if ($env:SNIFFERCOMMIT_FORCE_BUILD) { [bool][int]$env:SNIFFERCOMMIT_FORCE_BUILD } else { $false }
$script:PreferBuild   = if ($env:SNIFFERCOMMIT_PREFER_BUILD) { [bool][int]$env:SNIFFERCOMMIT_PREFER_BUILD } else { $false }
$script:ProxyUrl     = if ($env:HTTPS_PROXY) { $env:HTTPS_PROXY } elseif ($env:HTTP_PROXY) { $env:HTTP_PROXY } else { "" }

$script:UseColor = (-not $script:NoColor) -and ($Host -and $Host.UI -and $Host.UI.RawUI)

function Write-Info($Message) {
    if ($script:UseColor) { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
    else { Write-Host "[INFO] $Message" }
}

function Write-Success($Message) {
    if ($script:UseColor) { Write-Host "[OK] $Message" -ForegroundColor Green }
    else { Write-Host "[OK] $Message" }
}

function Write-Warn($Message) {
    if ($script:UseColor) { Write-Warning "[WARN] $Message" }
    else { Write-Warning "[WARN] $Message" }
}

function Write-Err($Message) {
    if ($script:UseColor) { Write-Host "[ERROR] $Message" -ForegroundColor Red }
    else { Write-Host "[ERROR] $Message" }
}

function Write-Step($Message) {
    if ($script:UseColor) { Write-Host "==> $Message" -ForegroundColor White }
    else { Write-Host "==> $Message" }
}

if ($Help) {
    Write-Host @"
sniffercommit Installer for Windows
Fast, C++20-powered pre-commit hook and CI generator.

USAGE:
    irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex

PARAMETERS:
    -Version <tag>       Install specific version (default: latest)
    -InstallDir <path>   Installation directory (default: auto-detect)
    -NoModifyPath        Do not add to PATH
    -Force               Force reinstall even if already installed
    -Uninstall           Remove sniffercommit from system

ENVIRONMENT VARIABLES:
    SNIFFERCOMMIT_VERSION       Version tag to install (e.g., v1.0.0)
    SNIFFERCOMMIT_INSTALL_DIR   Custom installation directory
    SNIFFERCOMMIT_FORCE_BUILD   Force build from source (1=on, 0=off)
    SNIFFERCOMMIT_PREFER_BUILD  Prefer build from source over release binary
    SNIFFERCOMMIT_BRANCH        Git branch for source builds (default: develop)
    HTTPS_PROXY / HTTP_PROXY    Proxy server for downloads
    NO_COLOR                    Disable colored output (1=on, 0=off)

EXAMPLES:
    # Install latest
    irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex

    # Install specific version
    $env:SNIFFERCOMMIT_VERSION="v1.0.0"
    irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex

    # Install to custom directory
    irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex -InstallDir "C:\Tools"

    # Uninstall
    irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex -Uninstall
"@
    exit 0
}

$script:TempDir = $null

function Remove-TempDir {
    if ($script:TempDir -and (Test-Path $script:TempDir)) {
        Remove-Item -Recurse -Force $script:TempDir -ErrorAction SilentlyContinue
    }
}

function Exit-WithError($Message) {
    Write-Err $Message
    Remove-TempDir
    exit 1
}

trap {
    Write-Err "Installation failed: $_"
    Write-Info "For help: $script:RepoUrl/issues"
    Write-Info "To inspect the script: irm $script:RepoUrl/raw/$script:Branch/install.ps1 | Out-String"
    Remove-TempDir
    exit 1
}

function Get-PlatformInfo {
    $script:Platform = "windows"
    $script:Arch = "x86_64"

    switch ($env:PROCESSOR_ARCHITECTURE) {
        "ARM64"  { $script:Arch = "arm64" }
        "AMD64"  { $script:Arch = "x86_64" }
        "x86"    { $script:Arch = "i686" }
        "ARM"    { $script:Arch = "arm" }
    }

    if ($env:PROCESSOR_ARCHITEW6432 -eq "AMD64") {
        $script:Arch = "x86_64"
    }

    Write-Info "Platform: $script:Platform ($script:Arch)"
}

function Test-Command($Name) {
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-DefaultInstallDir {
    if ($script:TargetDir) { return $script:TargetDir }

    $candidates = @(
        "$env:LOCALAPPDATA\Programs\$script:AppName"
        "$env:USERPROFILE\.local\bin"
        "$env:APPDATA\$script:AppName"
    )

    foreach ($dir in $candidates) {
        $parent = Split-Path $dir -Parent
        if (-not (Test-Path $parent)) { continue }

        try {
            New-Item -ItemType Directory -Force -Path $dir | Out-Null
            $testFile = Join-Path $dir ".write_test"
            [System.IO.File]::WriteAllText($testFile, "test")
            Remove-Item $testFile -ErrorAction SilentlyContinue
            return $dir
        } catch {
            continue
        }
    }

    $fallback = "$PWD\$script:AppName"
    New-Item -ItemType Directory -Force -Path $fallback | Out-Null
    return $fallback
}

function Add-ToUserPath($Dir) {
    if ($NoModifyPath) { return }

    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = $currentPath -split ";"

    if ($paths -contains $Dir) {
        Write-Info "Already in PATH: $Dir"
        return
    }

    Write-Step "Adding to PATH"
    try {
        [Environment]::SetEnvironmentVariable("Path", "$currentPath;$Dir", "User")
        Write-Success "Added to user PATH"
        Write-Info "Restart your terminal or run: `$env:Path += `";$Dir`""
    } catch {
        Write-Warn "Could not add to PATH automatically. Add manually: $Dir"
    }
}

function Remove-FromUserPath($Dir) {
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = $currentPath -split ";" | Where-Object { $_ -ne $Dir }
    [Environment]::SetEnvironmentVariable("Path", ($paths -join ";"), "User")
}

function Invoke-FileDownload($Url, $OutFile) {
    $params = @{
        Uri             = $Url
        OutFile         = $OutFile
        UseBasicParsing = $true
        ErrorAction     = "Stop"
    }

    if ($script:ProxyUrl) {
        $params['Proxy'] = $script:ProxyUrl
    }

    try {
        Invoke-WebRequest @params
    } catch {
        # Fallback to BITS for large files or restricted environments
        if (Test-Command "Start-BitsTransfer") {
            $bitsParams = @{
                Source      = $Url
                Destination = $OutFile
                ErrorAction = "Stop"
            }
            if ($script:ProxyUrl) { $bitsParams['Proxy'] = $script:ProxyUrl }
            Start-BitsTransfer @bitsParams
        } else {
            throw
        }
    }
}

function Get-LatestReleaseTag {
    Write-Step "Fetching latest release info"

    try {
        $headers = @{ "User-Agent" = "sniffercommit-installer" }
        $irmParams = @{
            Uri         = "$script:ReleaseApi/latest"
            Headers     = $headers
            ErrorAction = "Stop"
        }
        if ($script:ProxyUrl) { $irmParams['Proxy'] = $script:ProxyUrl }

        $release = Invoke-RestMethod @irmParams
        return $release.tag_name
    } catch {
        Write-Warn "Could not fetch latest version from GitHub API: $_"
        return $null
    }
}

function Get-ReleaseAsset($Version) {
    $tag = if ($Version -eq "latest") { Get-LatestReleaseTag } else { $Version }
    if (-not $tag) { return $null }

    Write-Info "Resolving version: $tag"

    try {
        $headers = @{ "User-Agent" = "sniffercommit-installer" }
        $irmParams = @{
            Uri         = "$script:ReleaseApi/tags/$tag"
            Headers     = $headers
            ErrorAction = "Stop"
        }
        if ($script:ProxyUrl) { $irmParams['Proxy'] = $script:ProxyUrl }

        $release = Invoke-RestMethod @irmParams

        $exactPattern = "$script:AppName-.*-$script:Platform-$script:Arch\.(zip|exe|tar\.gz)$"
        $asset = $release.assets | Where-Object { $_.name -match $exactPattern } | Select-Object -First 1

        if (-not $asset) {
            $fallbackPattern = "windows.*($script:Arch|x64|amd64|win64)"
            $asset = $release.assets | Where-Object {
                $_.name -match $fallbackPattern -and $_.name -match "\.(zip|exe|tar\.gz)$"
            } | Select-Object -First 1
        }

        if (-not $asset) {
            $asset = $release.assets | Where-Object {
                $_.name -match "windows" -and $_.name -match "\.(zip|exe|tar\.gz)$"
            } | Select-Object -First 1
        }

        if ($asset) {
            return @{
                Url  = $asset.browser_download_url
                Name = $asset.name
                Size = $asset.size
                Tag  = $tag
            }
        }
    } catch {
        Write-Warn "Could not fetch release assets: $_"
    }

    return $null
}

function Install-FromRelease($Asset) {
    Write-Step "Downloading $script:AppName $($Asset.Tag)"

    $script:TempDir = Join-Path $env:TEMP "$script:AppName-install-$(Get-Random)"
    New-Item -ItemType Directory -Force -Path $script:TempDir | Out-Null

    $downloadPath = Join-Path $script:TempDir $Asset.Name

    Write-Info "Downloading from GitHub releases..."
    Invoke-FileDownload -Url $Asset.Url -OutFile $downloadPath

    $size = (Get-Item $downloadPath).Length
    if ($Asset.Size -gt 0 -and $size -ne $Asset.Size) {
        Write-Warn "Download size mismatch: expected $($Asset.Size), got $size"
    }

    Write-Success "Downloaded $([math]::Round($size / 1MB, 2)) MB"

    $installDir = Get-DefaultInstallDir
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null

    if ($Asset.Name -match "\.zip$") {
        Write-Info "Extracting archive..."
        Expand-Archive -Path $downloadPath -DestinationPath $script:TempDir -Force

        $exe = Get-ChildItem -Path $script:TempDir -Recurse -Filter "*.exe" | Select-Object -First 1
        if ($exe) {
            Copy-Item $exe.FullName -Destination (Join-Path $installDir "$script:AppName.exe") -Force
        } else {
            Get-ChildItem -Path $script:TempDir -Exclude $Asset.Name | Copy-Item -Destination $installDir -Recurse -Force
        }
    } elseif ($Asset.Name -match "\.tar\.gz$|\.tgz$") {
        if (-not (Test-Command "tar")) {
            Exit-WithError "tar is required to extract .tar.gz files but was not found"
        }
        tar -xzf $downloadPath -C $script:TempDir
        $exe = Get-ChildItem -Path $script:TempDir -Recurse -Filter "*.exe" | Select-Object -First 1
        if ($exe) {
            Copy-Item $exe.FullName -Destination (Join-Path $installDir "$script:AppName.exe") -Force
        }
    } else {
        Copy-Item $downloadPath -Destination (Join-Path $installDir "$script:AppName.exe") -Force
    }

    $installedExe = Join-Path $installDir "$script:AppName.exe"
    if (-not (Test-Path $installedExe)) {
        Exit-WithError "Installation failed: executable not found at $installedExe"
    }

    try {
        $versionOutput = & $installedExe --version 2>$null
        Write-Success "Installed $script:AppName $versionOutput"
    } catch {
        Write-Warn "Could not verify installation, but files are in place"
    }

    Add-ToUserPath $installDir
    Remove-TempDir

    Write-Host ""
    Write-Success "$script:AppName installed successfully!"
    Write-Info "Location: $installDir"
    Write-Info "Run: $script:AppName --help"
}

function Get-CompilerInfo {
    $info = @{
        HasMSVC  = $false
        HasGCC   = Test-Command "gcc"
        HasClang = Test-Command "clang"
        HasNinja = Test-Command "ninja"
        VSPath   = $null
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($vsPath) {
            $info.HasMSVC = $true
            $info.VSPath = $vsPath
        }
    }

    return $info
}

function Select-CMakeGenerator($CompilerInfo) {
    if ($CompilerInfo.HasNinja -and $CompilerInfo.HasMSVC) {
        return "Ninja", $null
    }
    if ($CompilerInfo.HasNinja -and $CompilerInfo.HasGCC) {
        return "Ninja", $null
    }
    if ($CompilerInfo.HasMSVC) {
        return "Visual Studio 17 2022", "x64"
    }
    if ($CompilerInfo.HasGCC) {
        return "MinGW Makefiles", $null
    }
    if ($CompilerInfo.HasClang) {
        return "MinGW Makefiles", $null
    }

    return $null, $null
}

function Install-FromSource {
    Write-Step "Building from source"

    if (-not (Test-Command "cmake")) {
        Exit-WithError "CMake is required to build from source but was not found.`nInstall from: https://cmake.org/download/"
    }
    if (-not (Test-Command "git")) {
        Exit-WithError "Git is required to build from source but was not found.`nInstall from: https://git-scm.com/download/win"
    }

    $cmakeVersion = (& cmake --version | Select-Object -First 1)
    Write-Info "Found: $cmakeVersion"

    $compiler = Get-CompilerInfo
    $generator, $arch = Select-CMakeGenerator $compiler

    if (-not $generator) {
        Exit-WithError "No C++ compiler found. Install Visual Studio Build Tools or MinGW.`nSee: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022"
    }

    if ($compiler.HasMSVC) { Write-Info "Found MSVC at: $($compiler.VSPath)" }
    if ($compiler.HasGCC) { Write-Info "Found MinGW GCC" }
    if ($compiler.HasClang) { Write-Info "Found Clang" }
    Write-Info "Using generator: $generator"

    $script:TempDir = Join-Path $env:TEMP "$script:AppName-build-$(Get-Random)"
    New-Item -ItemType Directory -Force -Path $script:TempDir | Out-Null

    $sourceDir = Join-Path $script:TempDir $script:RepoName

    Write-Info "Cloning repository (branch: $script:Branch)..."
    & git clone --depth 1 --branch $script:Branch "$script:RepoUrl.git" $sourceDir
    if ($LASTEXITCODE -ne 0) {
        Exit-WithError "Failed to clone repository"
    }

    Write-Info "Configuring with CMake..."
    $buildDir = Join-Path $sourceDir "build"
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    $cmakeArgs = @("-S", $sourceDir, "-B", $buildDir, "-G", $generator)

    if ($arch) {
        $cmakeArgs += "-A", $arch
    }

    if ($generator -notmatch "Visual Studio") {
        $cmakeArgs += "-DCMAKE_BUILD_TYPE=Release"
    }

    $cmakeArgs += "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"

    if ($generator -eq "MinGW Makefiles" -and $compiler.HasGCC) {
        $mingwMake = Get-Command "mingw32-make" -ErrorAction SilentlyContinue
        if (-not $mingwMake) {
            $mingwMake = Get-Command "make" -ErrorAction SilentlyContinue
        }
        if ($mingwMake) {
            $cmakeArgs += "-DCMAKE_MAKE_PROGRAM=$($mingwMake.Source)"
        }
    }

    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        $cachedGen = Select-String -Path $cacheFile -Pattern "CMAKE_GENERATOR:INTERNAL=(.+)" | ForEach-Object { $_.Matches.Groups[1].Value }
        if ($cachedGen -and $cachedGen -ne $generator) {
            Write-Warn "Stale CMake cache detected (generator: $cachedGen). Cleaning..."
            Remove-Item -Recurse -Force $buildDir
            New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
        }
    }

    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Exit-WithError "CMake configuration failed. If you see 'generator mismatch', run with a clean build directory."
    }

    Write-Info "Building..."
    $buildArgs = @("--build", $buildDir, "--parallel")
    if ($generator -match "Visual Studio") {
        $buildArgs += "--config", "Release"
    }

    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        Exit-WithError "Build failed"
    }

    $installDir = Get-DefaultInstallDir
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null

    $builtExe = Join-Path $buildDir "Release\$script:AppName.exe"
    if (-not (Test-Path $builtExe)) {
        $builtExe = Join-Path $buildDir "$script:AppName.exe"
    }
    if (-not (Test-Path $builtExe)) {
        $builtExe = Join-Path $buildDir "bin\$script:AppName.exe"
    }
    if (-not (Test-Path $builtExe)) {
        $builtExe = Get-ChildItem -Path $buildDir -Recurse -Filter "$script:AppName.exe" |
            Select-Object -First 1
    }

    if ($builtExe -and (Test-Path $builtExe)) {
        Copy-Item $builtExe -Destination (Join-Path $installDir "$script:AppName.exe") -Force
    } else {
        Exit-WithError "Could not find built executable"
    }

    $compileDb = Join-Path $buildDir "compile_commands.json"
    $rootDb = Join-Path $sourceDir "compile_commands.json"
    if ((Test-Path $compileDb) -and -not (Test-Path $rootDb)) {
        try {
            New-Item -ItemType SymbolicLink -Path $rootDb -Target $compileDb -ErrorAction SilentlyContinue | Out-Null
        } catch {
            Copy-Item $compileDb -Destination $rootDb -Force -ErrorAction SilentlyContinue
        }
    }

    Add-ToUserPath $installDir
    Remove-TempDir

    Write-Host ""
    Write-Success "$script:AppName built and installed successfully!"
    Write-Info "Location: $installDir"
}

function Uninstall-App {
    Write-Step "Uninstalling $script:AppName"

    $installDir = $null
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\$script:AppName"
        "$env:USERPROFILE\.local\bin"
        "$env:APPDATA\$script:AppName"
        "$PWD\$script:AppName"
    )

    foreach ($dir in $candidates) {
        if (Test-Path (Join-Path $dir "$script:AppName.exe")) {
            $installDir = $dir
            break
        }
    }

    if (-not $installDir) {
        $pathDirs = $env:Path -split ";"
        foreach ($dir in $pathDirs) {
            if ($dir -and (Test-Path (Join-Path $dir "$script:AppName.exe"))) {
                $installDir = $dir
                break
            }
        }
    }

    if (-not $installDir) {
        Exit-WithError "$script:AppName not found. Is it installed?"
    }

    Write-Info "Found installation at: $installDir"

    $exePath = Join-Path $installDir "$script:AppName.exe"
    if (Test-Path $exePath) {
        Remove-Item $exePath -Force
        Write-Success "Removed: $exePath"
    }

    Remove-FromUserPath $installDir
    Write-Success "Removed from PATH"

    $remaining = Get-ChildItem $installDir -ErrorAction SilentlyContinue
    if (-not $remaining) {
        Remove-Item $installDir -Force
        Write-Success "Removed empty directory: $installDir"
    }

    Write-Host ""
    Write-Success "$script:AppName uninstalled successfully"
}

Write-Host @"
sniffercommit
"@ -ForegroundColor Cyan

Write-Host "Installer for $script:RepoUrl" -ForegroundColor Gray
Write-Host ""

if ($Uninstall) {
    Uninstall-App
    exit 0
}

Get-PlatformInfo

$existing = Get-Command $script:AppName -ErrorAction SilentlyContinue
if ($existing -and -not $Force) {
    $version = & $script:AppName --version 2>$null
    Write-Warn "$script:AppName is already installed ($version)"
    Write-Info "Use -Force to reinstall or -Uninstall to remove"
    exit 0
}

$asset = $null
if (-not $script:ForceBuild -and -not $script:PreferBuild) {
    $asset = Get-ReleaseAsset $script:TargetVersion
}

if ($asset -and -not $script:PreferBuild) {
    Install-FromRelease $asset
} else {
    if ($script:PreferBuild) {
        Write-Info "Preferring build from source (SNIFFERCOMMIT_PREFER_BUILD=1)"
    } elseif ($script:ForceBuild) {
        Write-Info "Forced build from source (SNIFFERCOMMIT_FORCE_BUILD=1)"
    } else {
        Write-Info "No prebuilt binary found for $script:Platform-$script:Arch"
    }
    Install-FromSource
}
