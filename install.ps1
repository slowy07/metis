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
    } else {
        "latest"
    }
}

echo "test"

if (-not $InstallDir) {
    $InstallDir = if ($env:SNIFFERCOMMIT_INSTALL_DIR) {
        $env:SNIFFERCOMMIT_INSTALL_DIR
    } else {
        ""
    }
}

$NO_COLOR = if ($env:NO_COLOR) {
    [int]$env:NO_COLOR
} else {
    0
}

$FORCE_BUILD = if ($env:SNIFFERCOMMIT_PREFER_BUILD) {
    [int]$env:SNIFFERCOMMIT_FORCE_BUILD
} else {
    0
}

$PROXY = if ($env:HTTPS_PROXY) {
    $env:HTTPS_PROXY
} elseif ($env:HTTP_PROXY) {
    $env:HTTP_PROXY
} else {
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

function Detect-Platform {
    $script:PLATFORM = "windows"
    $script:ARCH = "x86_64"

    if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") {
        $script:ARCH = "arm64"
    } elseif ($env:PROCESSOR_ARCHITECTURE -eq "AMD64") {
        $script:ARCH = "x86_64"
    } elseif ($env:PROCESSOR_ARCHITECTURE -eq "x86") {
        $script:ARCH = "i686"
    }

    Write-Info "Platform: $script:PLATFORM ($script:ARCH)"
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
            } catch {
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
    $paths = $currentPath - split ";"

    if ($paths -contains $Dir){
        Write-Info "Already in Path: $Dir"
    }
}
