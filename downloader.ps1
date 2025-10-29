#Requires -Version 5.0
<#
.SYNOPSIS
    Downloads and extracts Cangjie stdx release assets.
.DESCRIPTION
    This script downloads a specified version and platform-architecture of the Cangjie stdx library from its GitCode releases.
    It caches the downloaded file and extracts it to a specified directory.
.PARAMETER Version
    The version number to download (e.g., '1.0.3.1'). This is a mandatory parameter.
.PARAMETER Platform
    Alias 'p'. The platform and architecture combination (e.g., 'windows-x64'). If not specified, it defaults to the current system's platform and architecture.
    Supported values: linux-aarch64, linux-x64, mac-aarch64, mac-x64, ohos-aarch64, ohos-x64, windows-x64.
.PARAMETER Destination
    Alias 'd'. The directory where the file will be extracted. Defaults to the current directory.
.EXAMPLE
    .\downloader.ps1 -Version 1.0.3.1
    Downloads the latest version for the current platform and extracts to the current directory.
.EXAMPLE
    .\downloader.ps1 -Version 1.0.1.2 -Platform windows-x64 -Destination C:\libs
    Downloads the windows-x64 version 1.0.1.2 and extracts it to C:\libs.
#>
param (
    [Parameter(Mandatory = $true, Position=0)]
    [string]$Version,

    [Parameter(Mandatory = $false)]
    [Alias('p')]
    [ValidateSet('linux-aarch64', 'linux-x64', 'mac-aarch64', 'mac-x64', 'ohos-aarch64', 'ohos-x64', 'windows-x64')]
    [string]$Platform,

    [Parameter(Mandatory = $false)]
    [Alias('d')]
    [string]$Destination = '.'
)

# --- Helper Functions ---
function Get-PlatformArch {
    $os = $env:OS
    $arch = $env:PROCESSOR_ARCHITECTURE

    if ($os -eq 'Windows_NT') {
        $platform = 'windows'
    } else {
        # For PowerShell on non-Windows, use 'uname'
        $unameS = (uname -s).ToLower()
        if ($unameS -eq 'linux') {
            $platform = 'linux'
        } elseif ($unameS -eq 'darwin') {
            $platform = 'mac'
        } else {
            throw "Unsupported operating system: $unameS"
        }
    }

    if ($IsCoreCLR) { # PowerShell 6+
        $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture
        switch ($arch) {
            'X64' { $architecture = 'x64' }
            'Arm64' { $architecture = 'aarch64' }
            default { throw "Unsupported architecture: $arch" }
        }
    } else { # Windows PowerShell 5.1
         switch ($arch) {
            'AMD64' { $architecture = 'x64' }
            # Add other architectures if needed for older PS versions
            default { throw "Unsupported architecture: $arch" }
        }
    }


    return "${platform}-${architecture}"
}

# --- Main Script ---
try {
    # 1. Argument Defaulting
    if (-not $PSBoundParameters.ContainsKey('Platform')) {
        $Platform = Get-PlatformArch
    }

    # 2. Define Variables
    $RepoUrl = "https://gitcode.com/Cangjie/cangjie_stdx"
    $FileName = "cangjie-stdx-${Platform}-${Version}.zip"
    $DownloadUrl = "$RepoUrl/releases/download/v${Version}/${FileName}"
    $CacheDir = Join-Path $HOME ".cangjie_stdx/v${Version}"
    $CachedFile = Join-Path $CacheDir $FileName
    $ExtractedDirName = "cangjie-stdx-${Platform}-${Version}"

    # 3. Destination Directory Check
    $DestDirAbs = Resolve-Path -Path $Destination
    if (-not (Test-Path -Path $DestDirAbs -PathType Container)) {
        throw "Destination directory does not exist: $DestDirAbs"
    }

    # 4. Cache and Download Logic
    if (-not (Test-Path -Path $CacheDir -PathType Container)) {
        New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    }

    if (Test-Path -Path $CachedFile) {
        Write-Host "Using cached file: $CachedFile"
    } else {
        Write-Host "Downloading from: $DownloadUrl"
        # Using Invoke-WebRequest for better progress and error handling
        Invoke-WebRequest -Uri $DownloadUrl -OutFile $CachedFile -UseBasicParsing
        if ($LASTEXITCODE -ne 0) {
            Remove-Item -Path $CachedFile -ErrorAction SilentlyContinue
            throw "Download failed."
        }
    }

    # 5. Extraction and Rename Logic
    Write-Host "Extracting to: $DestDirAbs"
    $FinalPath = Join-Path $DestDirAbs $ExtractedDirName

    if (Test-Path -Path $FinalPath) {
        Write-Warning "Target directory already exists. Overwriting: $FinalPath"
        Remove-Item -Recurse -Force -Path $FinalPath
    }

    # Expand-Archive creates the destination directory
    Expand-Archive -Path $CachedFile -DestinationPath $FinalPath -Force

    # 6. Success Output
    Write-Host "Successfully extracted to: $FinalPath"

} catch {
    Write-Error "Error: $($_.Exception.Message)"
    exit 1
}
