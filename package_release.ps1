$ErrorActionPreference = "Stop"

$ReleaseDir = "Vrutti_Release"
$ZipFile = "vrutti_early_release.zip"

Write-Host "Cleaning up old release folder..."
if (Test-Path $ReleaseDir) { Remove-Item -Recurse -Force $ReleaseDir }
if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }

Write-Host "Creating release structure..."
New-Item -ItemType Directory -Force -Path "$ReleaseDir" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\builtin-themes" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ui\frontend\dist" | Out-Null

Write-Host "Copying Vrutti executable and DLLs..."
if (Test-Path "build\vrutti_app.exe") {
    Copy-Item "build\vrutti_app.exe" "$ReleaseDir\vrutti.exe"
} else {
    Write-Host "ERROR: build\vrutti_app.exe not found! Compile the project first."
    exit 1
}
if (Test-Path "WebView2Loader.dll") {
    Copy-Item "WebView2Loader.dll" "$ReleaseDir\"
}
if (Test-Path "libvrutti_search.dll") {
    Copy-Item "libvrutti_search.dll" "$ReleaseDir\"
}
if (Test-Path "build\WebView2Loader.dll") {
    Copy-Item "build\WebView2Loader.dll" "$ReleaseDir\"
}
if (Test-Path "build\libvrutti_search.dll") {
    Copy-Item "build\libvrutti_search.dll" "$ReleaseDir\"
}

Write-Host "Copying Node backend (src\ext)..."
Copy-Item "src\ext\*.js" "$ReleaseDir\src\ext\"
if (Test-Path "src\ext\bin\node.exe") {
    Copy-Item "src\ext\bin\node.exe" "$ReleaseDir\src\ext\bin\"
}
if (Test-Path "src\ext\builtin-themes") {
    Copy-Item "src\ext\builtin-themes\*" "$ReleaseDir\src\ext\builtin-themes\" -Recurse
}

Write-Host "Copying UI frontend bundle (src\ui\frontend\dist)..."
if (Test-Path "src\ui\frontend\dist") {
    Copy-Item "src\ui\frontend\dist\*" "$ReleaseDir\src\ui\frontend\dist\" -Recurse
} else {
    Write-Host "ERROR: src\ui\frontend\dist not found! Run frontend build first."
    exit 1
}

Write-Host "Compressing to $ZipFile..."
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipFile

Write-Host "Done! Release package created at $ZipFile"
