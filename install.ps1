# sniffercommit install script for Windows
# Usage: powershell -ExecutionPolicy ByPass -c "irm https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.ps1 | iex"

param(
    [string]$Version = "latest",
    [string]$InstallDir = "",
    [switch]$NoModifyPath,
    [switch]$Force,
    [switch]$Help,
    [switch]$Uninstall
)

if ($Help) {
    Write-Output "sniffercommit Windows install script"
    Write-Output ""
    Write-Output "Usage:"
    Write-Output "  irm https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.ps1 | iex"
    exit 0
}

if ($Uninstall) {
    Write-Output "Uninstall not yet implemented"
    exit 0
}

if (-not $InstallDir) {
    $InstallDir = Join-Path $HOME ".local\bin"
}

$Repo = "slowy07/sniffercommit"

$Arch = $env:PROCESSOR_ARCHITECTURE
if ($Arch -ne "AMD64") {
    Write-Error "Unsupported architecture: $Arch"
    exit 1
}

if ($Version -eq "latest") {
    $BaseUrl = "https://github.com/$Repo/releases/latest/download"
} else {
    $BaseUrl = "https://github.com/$Repo/releases/download/$Version"
}

$DownloadUrl = "$BaseUrl/sniffercommit-windows-x86_64.zip"

Write-Output "Downloading sniffercommit for windows-x86_64..."

$TempDir = Join-Path $env:TEMP "sniffercommit_install_$(Get-Random)"
$ZipPath = Join-Path $TempDir "sniffercommit.zip"

New-Item -ItemType Directory -Force -Path $TempDir | Out-Null

try {
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath -UseBasicParsing

    Expand-Archive -Path $ZipPath -DestinationPath $TempDir -Force

    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Move-Item -Path (Join-Path $TempDir "sniffercommit.exe") -Destination (Join-Path $InstallDir "sniffercommit.exe") -Force

    Write-Output "sniffercommit installed to $InstallDir\sniffercommit.exe"

    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath -notlike "*$InstallDir*") {
        Write-Output ""
        Write-Output "  $InstallDir is not in your PATH."
        Write-Output "  Add it manually or run:"
        Write-Output "    [Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path','User') + ';$InstallDir', 'User')"
    }

    Write-Output "Run 'sniffercommit --help' to get started."
}
finally {
    Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue
}
