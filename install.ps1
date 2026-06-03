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

$APP_NAME = "sniffercommit"
$REPO_OWNER = "slowy07"
$REPO_NAME = "sniffercommit"
$REPO_URL = "https://github.com/$REPO_OWNER/$REPO_NAME"
$RELEASE_API = "https://api.github.com/repos/$REPO_OWNER/$REPO_NAME/releases"

if (-not $Version) {
    $Version = if ($env:SNIFFERCOMMIT_VERSION) {
        $env:SNIFFERCOMMIT_VERSION
    }
    else {
        "latest"
    }
}

if (-not $InstallDir) {
    $InstallDir = if ($env:SNIFFERCOMMIT_INSTALL_DIR) {
        $env:SNIFFERCOMMIT_INSTALL_DIR
    }
    else {
        ""
    }
}

$NO_COLOR = if ($env:NO_COLOR) {
    [int]$env:NO_COLOR
}
else {
    0
}

$FORCE_BUILD = if ($env:SNIFFERCOMMIT_PREFER_BUILD) {
    [int]$env:SNIFFERCOMMIT_FORCE_BUILD
}
else {
    0
}

$PROXY = if ($env:HTTPS_PROXY) {
    $env:HTTPS_PROXY
}
elseif ($env:HTTP_PROXY) {
    $env:HTTP_PROXY
}
else {
    ""
}

$script:USE_COLOR = (-not $NO_COLOR) -and ($Host -and $Host.UI -and $Host.UI.RawUI)

function Write-Info($Message) {
    if ($script:USE_COLOR) { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
    else { Write-Host "[INFO] $Message" }
}

function Write-Success($Message) {
    if ($script:USE_COLOR) { Write-Host "[OK] $Message" -ForegroundColor Green }
    else { Write-Host "[OK] $Message" }
}

function Write-Warn($Message) {
    if ($script:USE_COLOR) { Write-Warning "[WARN] $Message" }
    else { Write-Warning "[WARN] $Message" }
}

function Write-Error($Message) {
    if ($script:USE_COLOR) { Write-Host "[ERROR] $Message" -ForegroundColor Red }
    else { Write-Host "[ERROR] $Message" }
}

function Write-Step($Message) {
    if ($script:USE_COLOR) { Write-Host "==> $Message" -ForegroundColor White }
    else { Write-Host "==> $Message" }
}


if ($Help) {
    Write-Host @"
Sniffercommit installer for windows

Fast, C++20-powered pre-commit hook and ci generator
USAGE:
irm https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.ps1 | iex

PARAMETER:
-Version <tag> Install specific version (default: latest)
-InstallDir <path> Installation directory (default: auto-detect)
-NoModifyPath Do not add to PATH
-Force Force reinstall even if already installed
-Uninstall Remove sniffercommit from system
"@
    exit 0
}

$script:TEMP_DIR = $null

function Cleanup {
    if ($script:TEMP_DIR -and (Test-Path $script:TEMP_DIR)) {
        Remove-Item -Recurse -Force $script:TEMP_DIR -ErrorAction SilentlyContinue
    }
}

function Die($Message) {
    Write-Erorr $Message
    Cleanup
    exit 1
}

trap {
    Write-Error "installation Failed: $_"
    Write-Info "For Information: $REPO_URL/issues"
    Write-Info "For inspect script: irm $REPO_URL/raw/main/install.ps1 | Out-String"
    Cleanup
    exit 1
}

function Get-Platform {
    $script:PLATFORM = "windows"
    $script:ARCH = "x86_64"

    if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
        $script:ARCH = "arm64"
    }
    elseif ($env:PROCESSOR_ARCHITECTURE -eq "AMD64") {
        $script:ARCH = "x86_64"
    }
    elseif ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
        $script:ARCH = "i686"
    }

    Write-Info "Platform: $script:PLATFORM ($script:ARCH)"
}

function Has-Command($Command) {
    return [bool](Get-Command $command -ErrorAction SilentlyContinue)
}

function Get-InstallDir {
    if ($InstallDir) {
        return $InstallDir
    }

    $candidates = @(
        "$env:LOCALAPPDATA\Programs\$APP_NAME",
        "$env:USERPROFILE\.local\bin",
        "$env:USERPROFILE\bin",
        "$env:APPDATA\$APP_NAME"
    )

    foreach ($dir in $candidates) {
        $parent = Split-Path $dir-parent
        if (Test-Path $parent) {
            try {
                $testFile = Join-Path $dir ".write_test"
                New-Item -ItemType Directory -Force -Path $dir | Out-Null
                [System.IO.File]::WriteAllText($testFile, "test")
                Remove-Item $testFile -ErrorAction SilentlyContinue
                return $dir
            }
            catch {
                continue
            }
        }
    }

    return "$PWD\$APP_NAME"
}

function Add-ToPath($Dir) {
    if ($NoModifyPath) {
        return
    }

    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = $currentPath -split ";"

    if ($paths -contains $Dir) {
        Write-Info "Already in Path: $Dir"
    }

    Write-Step "Adding to PATH"

    try {
        [Environment]::SetEnvironmentVariable("Path", "$CurrentPath;$Dir", "User")
        Write-Success "Added to user PATH"
        Write-Info "Restart your terminal or run: \$env:Path += `";Dir`""
    }
    catch {
        Write-Warn "Could not add to PATH automatically, add manually: $Dir"
    }
}


function Remove-FromPath($Dir) {
    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $paths = $currentPath -split ";" | Where-Object { $_ -ne $Dir }
    [Environment]::SetEnvironmentVariable("Path", ($paths -join ";"), "User")
}

function Invoke-Download($Url, $OutFile) {
    $params = @{
        Uri             = $Url
        OutFile         = $OutFile
        UseBasicParsing = $true
        ErrorAction     = "Stop"
    }

    if ($PROXY) {
        $params['Proxy'] = $PROXY
    }

    try {
        Invoke-WebRequest @params
    }
    catch {
        if (Has-Command "Start-BitsTransfer") {
            Start-BitsTransfer -Source $Url -Destination $OutFile -ErrorAction Stop
        }
        else {
            throw
        }
    }
}

function Get-LatestVersion {
    Write-Step "Fetching latest release info"

    try {
        $headers = @{"User-Agent" = "sniffercommit-installer" }
        if ($PROXY) {
            $release = Invoke-RestMethod -Uri "$RELEASE_API/latest" -Headers $headers -Proxy $Proxy
        }
        else {
            $release = Invoke-RestMethod -Uri "$RELEASE_API/latest" -Headers $headers
        }

        return $release.tag_name
    }
    catch {
        Write-Warn "Cannot fetch latest version from GITHUB api"

        return $null
    }
}

function Get-ReleaseAsset($Version) {
    $tag = if ($Version -eq "latest" ) { Get-LatestVersion } else { $Version }
    if (-not $tag) { return $null }

    Write-Info "Resolving version: $tag"

    try {
        $headers = @{ "User-Agent" = "sniffercommit-installer" }
        $url = "$RELEASE_API/tags/$tag"

        if ($PROXY) {
            $release = Invoke-RestMethod -Uri $url -Headers $headers -Proxy $PROXY
        }
        else {
            $release = Invoke-RestMethod -Uri $url -Headers $headers
        }

        $assetPattern = "$APP_NAME-.*-$script:PLATFORM-$script:ARCH\.(zip|exe|tar\.gz)$"
        $asset = $release.assets | Where-Object { $_.name -match $assetPattern } | Select-Object -First 1

        if (-not $asset) {
            $asset = $release.assets | Where-Object {
                $_.name -match "windows" -and ($_.name -match $script:ARCH -or $_.name -match "x64|amd64")
            } | Select-Object -First 1
        }

        if ($asset) {
            return @{
                Url = $asset.browser_download_url
                Name = $asset.name
                Size = $asset.size
                Tag = $tag
            }
        }
        
    } catch {
        Write-Warn "Could not fetching release assets: $_"
    }

    return $null
}

function Install-FromRelease($Asset) {
    Write-Step "Download $APP_NAME $($Asset.Tag)"

    $script:TEMP_DIR = Join-Path $env:TEMP "$APP_NAME-install-$(Get-Random)"
    New-Item -ItemType Directory -Force -Path $script:TEMP_DIR | Out-Null
    
    $downloadPath = Join-Path $script:TEMP_DIR $Asset.Name

    Write-Info "Download from github release"
    Invoke-Download -Url $Asset.Url -OutFile $downloadPath

    $size = (Get-Item $downloadPath).Length
    if ($Asset.Size -gt 0 -and $size -and $size -ne $Asset.Size) {
        Write-Warn "Download size mismatch: expected $(Asset.Size), got $size"
    }

    Write-Success "Download $([math]::Round($size / 1MB, 2)) MB"

    $InstallDir = Get-InstallDir
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

    if ($Asset.Name -match "\.zip$") {
        Write-Info "Extract archive"
        Expand-Archive -Path $downloadPath -DestinationPath $script:TEMP_DIR -Force

        $exe = Get-ChildItem -Path $script:TEMP_DIR -Recurse -Filter "*.exe" | Select-Object -First 1
        
        if ($exe) {
            Copy-Item $exe.FullName -Destination (Join-Path $InstallDir "$APP_NAME.exe") -Force
        } else {
            Get-ChildItem -Path $script:TEMP_DIR -Exclude $Asset.Name | Copy-Item -Destination $InstallDir -Recurse -Force
        }
    } elseif ($Asset.Name -match "\.tar\.gz$|\.tgz$") {
        if (-not (Has-Command "tar")) {
            Die "tar is required to extract .tar.gz file but not found"
        }

        tar -xzf $downloadPath -C $script:TEMP_DIR
        $exe = Get-ChildItem -Path $script:TEMP_DIR -Recurse -Filter "*.exe" | Select-Object -First 1

        if ($exe) {
            Copy-Item $exe.FullName -Destination (Join-Path $installDir)
        }
    } else {
        Copy-Item $downloadPath -Destination (Join-Path $InstallDir "$APP_NAME.exe") -Force
    }

    $installedExe = Join-Path $installDir "$APP_NAME.exe"
    if (-not (Test-Path $installedExe )) {
        Die "Installation failed: executable not found at $installedExe"
    }

    try {
        $versionOutput = & $installedExe --verion 2>$null
        Write-Success "installed at $APP_NAME $versionOutput"   
    }
    catch {
        Write-Warn "could not verify installation, but files are in place"
    }

    Write-Host ""
    Write-Success "$APP_NAME installed succesfully"
    Write-Info "Location: $installDir"
    Write-Info "Run: $APP_NAME --help"
}

function Install-FromSource {
    Write-Step "Building from source"
    
    if (-not (Has-Command "cmake")) {
        Die "cmake is required to build from source but was not found. `nInstall from: https://cmake.org/download"
    }

    $cmakeVersion = & cmake --version | Select-Object -First 1
    Write-Info "Found: $cmakeVersion"

    $hasMSVC = $false
    $hasGCC = Has-Command "gcc"
    $hasClang = Has-Command "clang"

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            $hasMSVC = $true
            Write-Info "found MSVC at: $vsPath"
        }
    }

    if (-not ($hasMSVC -or $hasGCC -or $hasClang)) {
        Die "no C++ compile found, install visual studio build tools first"
    }

    $script:TEMP_DIR = Join-Path $env:TEMP "$APP_NAME-build-$(Get-Random)"
    New-Item -ItemType Directory -Force -Path $script:TEMP_DIR | Out-Null

    $branch = if ($env:SNIFFERCOMMIT_BRANCH) { $env:SNIFFERCOMMIT_BRANCH } else { "develop" }
    $sourceDir = Join-Path $script:TEMP_DIR $REPO_NAME

    Write-Info "cloning repository (branch: $branch)"
    & git clone --depth 1 --branch $branch "$REPO_URL.git" $sourceDir
    if ($LASTEXITCODE -ne 0) {
        Die "failed to clone repository"
    }

    Write-Info "configure with cmake"
    $buildDir = Join-Path $sourceDir "build"
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    $builtExe = Join-Path $buildDir "Release\$APP_NAME.exe"
    
    if (-not (Test-Path $builtExe)) {
        $builtExe = Join-Path $buildDir "$APP_NAME.exe"
    }

    if (Test-Path $builtExe) {
        Copy-Item $builtExe -Destination (Join-Path $installDir "$APP_NAME.exe") -Force
    } else {
        Die "could not find built executable"
    }

    Add-ToPath $installDir
    Cleanup

    Write-Host ""
    Write-Success "$APP_NAME built and installed success"
    Write-Info "location: $installDir"
}

Get-Platform

$existing = Get-Command $APP_NAME -ErrorAction SilentlyContinue
if ($existing -and -not $Force) {
    $version = & $APP_NAME --version 2>$null
    Write-Warn "$APP_NAME is already installed ($version)"
    Write-Info "use -Force to reinstall"
    exit 0
}

$asset = $null
if (-not $FORCE_BUILD -and -not $PREFER_BUILD) {
    $asset = Get-ReleaseAsset $Version
}

if ($asset -and -not $PREFER_BUILD) {
    Install-FromRelease $asset
} else {
    if ($PREFER_BUILD) {
        Write-Info "preferring build from source (SNIFFERCOMMIT_PREFER_BUILD=1)"
    } elseif ($FORCE_BUILD) {
        Write-Info "forced build from source (SNIFFERCOMMIT_FORCE_BUILD=1)"
    } else {
        Write-Info "no prebuilt binary found for $script:PLATFORM-$script:ARCH"
    }

    Install-FromSource
}
