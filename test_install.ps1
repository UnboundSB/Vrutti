$ErrorActionPreference = 'Stop'
$name = 'test'
$url = 'https://open-vsx.org/api/ms-python/python/2026.4.0/file/ms-python.python-2026.4.0.vsix'
$extDir = Join-Path $env:USERPROFILE '.vrutti\extensions\'
if (!(Test-Path $extDir)) { New-Item -ItemType Directory -Force -Path $extDir | Out-Null }
$destDir = Join-Path $extDir $name
if (Test-Path $destDir) { Remove-Item -Recurse -Force $destDir }
$tmpZip = Join-Path $env:TEMP "$name.zip"
Invoke-WebRequest -Uri $url -OutFile $tmpZip
Expand-Archive -Path $tmpZip -DestinationPath $destDir -Force
Remove-Item $tmpZip
Write-Output '{"type":"success"}'
