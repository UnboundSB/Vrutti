$ErrorActionPreference = "Stop"

$ReleaseDir = "Vrutti_Release"

Write-Host "Cleaning up old release folder..."
if (Test-Path $ReleaseDir) { Remove-Item -Recurse -Force $ReleaseDir }

Write-Host "Creating release structure..."
New-Item -ItemType Directory -Force -Path "$ReleaseDir" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\builtin-themes" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ui\frontend\dist" | Out-Null

Write-Host "Copying Vrutti executable..."
if (Test-Path "build\vrutti_app.exe") {
    Copy-Item "build\vrutti_app.exe" "$ReleaseDir\vrutti.exe"
    if (Test-Path "build\libvrutti_search.dll") {
        Copy-Item "build\libvrutti_search.dll" "$ReleaseDir\"
    }
} else {
    Write-Host "ERROR: build\vrutti_app.exe not found! Compile the project first."
    exit 1
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
    # Copy the dist folder itself into the frontend folder to preserve subdirectories correctly
    Copy-Item "src\ui\frontend\dist" "$ReleaseDir\src\ui\frontend" -Recurse -Force
} else {
    Write-Host "ERROR: src\ui\frontend\dist not found! Run frontend build first."
    exit 1
}

Write-Host "Release structure generated at $ReleaseDir."

# Try to find Inno Setup
$ISCC = "$env:ProgramFiles (x86)\Inno Setup 6\ISCC.exe"
if (-Not (Test-Path $ISCC)) {
    $ISCC = "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
}
if (-Not (Test-Path $ISCC)) {
    $ISCC = "$env:LOCALAPPDATA\Programs\Antigravity IDE\_\resources\app\node_modules\innosetup\bin\ISCC.exe"
}

if (Test-Path $ISCC) {
    Write-Host "Found Inno Setup at $ISCC. Compiling installer..."
    & $ISCC "installer\VruttiSetup.iss"
    Write-Host "Done! Installer created at Vrutti_Setup.exe"
} else {
    Write-Host "WARNING: Inno Setup compiler (ISCC.exe) not found!"
    Write-Host "The release folder was built, but the installer executable was not created."
    Write-Host "Please download and install Inno Setup from https://jrsoftware.org/isdl.php"
    Write-Host "Then run this script again."
}
