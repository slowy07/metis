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
    Write-Output "sniffercommit Windows install script`n`nUsage:`n  irm https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.ps1 | iex"
    exit 0
}

if ($Uninstall) {
    Write-Output "Uninstall not yet implemented"
    exit 0
}

if (-not $InstallDir) {
    $InstallDir = Join-Path $HOME ".local\bin"
}

$Arch = $env:PROCESSOR_ARCHITECTURE
if ($Arch -ne "AMD64") {
    Write-Error "Unsupported architecture: $Arch"
    exit 1
}

if ($Force -or $env:SNIFFERCOMMIT_FORCE_BUILD) {
    Write-Output "Building sniffercommit from source..."
    $SourceDir = Join-Path $env:TEMP "sniffercommit_source_$(Get-Random)"
    New-Item -ItemType Directory -Force -Path $SourceDir | Out-Null
    try {
        $Branch = if ($env:SNIFFERCOMMIT_BRANCH) { $env:SNIFFERCOMMIT_BRANCH } else { "main" }
        git clone --depth 1 --branch $Branch "https://github.com/slowy07/sniffercommit.git" $SourceDir 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Failed to clone repository"
            exit 1
        }
        Push-Location $SourceDir
        cmake -B build -DCMAKE_BUILD_TYPE=Release -DSNIFFERCOMMIT_BUILD_TESTS=OFF
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake configuration failed"
            exit 1
        }
        cmake --build build --config Release --parallel
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Build failed"
            exit 1
        }
        New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
        $exe = Get-ChildItem -Path (Join-Path $SourceDir "build\bin") -Recurse -Filter "sniffercommit.exe" | Select-Object -First 1
        if (-not $exe) {
            Write-Error "Build succeeded but binary not found"
            exit 1
        }
        Copy-Item $exe.FullName (Join-Path $InstallDir "sniffercommit.exe") -Force
        Pop-Location
    }
    finally {
        Remove-Item -Recurse -Force $SourceDir -ErrorAction SilentlyContinue
    }
    Write-Output "sniffercommit installed to $InstallDir\sniffercommit.exe"
} else {
    $Repo = "slowy07/sniffercommit"
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
        try {
            Invoke-WebRequest -Uri $DownloadUrl -OutFile $ZipPath -UseBasicParsing -ErrorAction Stop
        } catch {
            Write-Warning "No release found at $DownloadUrl"
            Write-Warning "Install a specific version or use -Force to build from source."
            exit 0
        }
        Expand-Archive -Path $ZipPath -DestinationPath $TempDir -Force
        New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
        Move-Item -Path (Join-Path $TempDir "sniffercommit.exe") -Destination (Join-Path $InstallDir "sniffercommit.exe") -Force
    }
    finally {
        Remove-Item -Recurse -Force $TempDir -ErrorAction SilentlyContinue
    }
    Write-Output "sniffercommit installed to $InstallDir\sniffercommit.exe"
}

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -notlike "*$InstallDir*") {
    Write-Output ""
    Write-Output "  $InstallDir is not in your PATH."
    Write-Output "  Add it manually or run:"
    Write-Output "    [Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path','User') + ';$InstallDir', 'User')"
}

Write-Output "Run 'sniffercommit --help' to get started."
