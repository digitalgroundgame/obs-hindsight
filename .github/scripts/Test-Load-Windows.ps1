[CmdletBinding()]
param(
    # The plugin zip from Package-Windows.ps1.
    [Parameter(Mandatory)]
    [string] $PluginZip,
    # How long OBS gets to load the plugin before the check fails.
    [int] $TimeoutSeconds = 120
)

# Starts the OBS Studio release we build against with the plugin installed
# where the installer puts it, and checks the OBS log to see that the plugin
# loaded. This catches what the build can't: a DLL the plugin needs that OBS
# doesn't ship, or a crash in obs_module_load.

$ErrorActionPreference = 'Stop'

if ( $env:CI -eq $null ) {
    throw "Test-Load-Windows.ps1 requires CI environment"
}

# OBS's own Windows zip for the version in buildspec.json. When that version
# changes, the download fails this check until the hash below is updated.
$ObsZipHashes = @{
    '31.1.1' = '9d8dceb77acd8af04af23f877061f63c9bef78ca73d2093d0ccba1bb9104173f'
}

$ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
$BuildSpec = Get-Content -Path "${ProjectRoot}/buildspec.json" -Raw | ConvertFrom-Json
$ProductName = $BuildSpec.name
$ObsVersion = $BuildSpec.dependencies.'obs-studio'.version

$ExpectedHash = $ObsZipHashes[$ObsVersion]
if ( $ExpectedHash -eq $null ) {
    throw "No OBS zip hash for OBS ${ObsVersion}. Add one to Test-Load-Windows.ps1."
}

$WorkDir = Join-Path $env:RUNNER_TEMP 'obs-load-check'
$ObsDir = Join-Path $WorkDir 'obs'
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

Write-Output "::group::Download OBS ${ObsVersion}"
$ObsZip = Join-Path $WorkDir 'obs.zip'
$ObsUrl = "https://github.com/obsproject/obs-studio/releases/download/${ObsVersion}/OBS-Studio-${ObsVersion}-Windows-x64.zip"
Invoke-WebRequest -Uri $ObsUrl -OutFile $ObsZip
$ActualHash = (Get-FileHash -Path $ObsZip -Algorithm SHA256).Hash.ToLower()
if ( $ActualHash -ne $ExpectedHash ) {
    throw "OBS zip hash mismatch: expected ${ExpectedHash}, got ${ActualHash}"
}
Expand-Archive -Path $ObsZip -DestinationPath $ObsDir -Force
Write-Output '::endgroup::'

Write-Output '::group::Install plugin'
# The same folder the installer uses (installer-Windows.iss).
$PluginsDir = Join-Path $env:ProgramData 'obs-studio\plugins'
$Unpacked = Join-Path $WorkDir 'plugin'
Expand-Archive -Path $PluginZip -DestinationPath $Unpacked -Force
New-Item -ItemType Directory -Force -Path $PluginsDir | Out-Null
Copy-Item -Path (Join-Path $Unpacked $ProductName) -Destination $PluginsDir -Recurse -Force
Get-ChildItem -Path (Join-Path $PluginsDir $ProductName) -Recurse | ForEach-Object { $_.FullName }
Write-Output '::endgroup::'

# Portable mode keeps OBS's config and logs in $ObsDir\config.
$ObsBin = Join-Path $ObsDir 'bin\64bit'
$LogDir = Join-Path $ObsDir 'config\obs-studio\logs'
$ObsArgs = @(
    '--portable'
    '--minimize-to-tray'
    '--disable-updater'
    '--disable-shutdown-check'
    '--disable-missing-files-check'
)
$Obs = Start-Process -FilePath (Join-Path $ObsBin 'obs64.exe') -WorkingDirectory $ObsBin -ArgumentList $ObsArgs -PassThru
# Reading the handle now keeps ExitCode readable after OBS exits.
$null = $Obs.Handle

# obs_log adds the plugin name, so this only matches our own line.
$Loaded = "[${ProductName}] plugin loaded successfully"
# libobs's messages when it can't open or start a module, naming its file.
$Failed = "(?i)failed[^\r\n]*${ProductName}\.dll"
# OBS has tried every module by the time it logs this.
$StartupDone = '==== Startup complete'
$LogFile = $null
$Result = $null
$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)

function Read-ObsLog {
    if ( $script:LogFile -eq $null ) {
        $script:LogFile = Get-ChildItem -Path $LogDir -Filter '*.txt' -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime | Select-Object -Last 1
    }
    if ( $script:LogFile -eq $null ) { return $null }
    # OBS may be mid-write; a failed read counts as nothing new yet.
    try { return Get-Content -Path $script:LogFile.FullName -Raw } catch { return $null }
}

while ( (Get-Date) -lt $Deadline ) {
    $Log = Read-ObsLog
    if ( $Log ) {
        if ( $Log -match $Failed ) { $Result = "OBS logged: $($Matches[0])"; break }
        if ( $Log.Contains($Loaded) ) { $Result = 'loaded'; break }
        if ( $Log.Contains($StartupDone) ) { $Result = 'OBS finished starting without loading it'; break }
    }
    if ( $Obs.HasExited ) { $Result = "OBS exited with code $($Obs.ExitCode)"; break }
    Start-Sleep -Seconds 2
}

# The update check starts after the loaded line, so give OBS a moment to
# show it didn't crash after that.
if ( $Result -eq 'loaded' ) {
    Start-Sleep -Seconds 5
    if ( $Obs.HasExited ) { $Result = "OBS exited with code $($Obs.ExitCode) after loading the plugin" }
}

if ( ! $Obs.HasExited ) {
    Stop-Process -Id $Obs.Id -Force
}

Write-Output '::group::OBS log'
$Log = Read-ObsLog
if ( $Log ) { Write-Output $Log } else { Write-Output "No log found in ${LogDir}" }
Write-Output '::endgroup::'

if ( $Result -ne 'loaded' ) {
    if ( $Result -eq $null ) { $Result = "timed out after ${TimeoutSeconds} s" }
    throw "${ProductName} did not load in OBS ${ObsVersion}: ${Result}. See the OBS log above."
}

Write-Output "${ProductName} loaded in OBS ${ObsVersion}."
