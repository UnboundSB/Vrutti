$ErrorActionPreference = "Stop"

$ReleaseDir = "Vrutti_Release"

Write-Host "Cleaning up old release folder..."
if (Test-Path $ReleaseDir) { Remove-Item -Recurse -Force $ReleaseDir }

Write-Host "Creating release structure..."
New-Item -ItemType Directory -Force -Path "$ReleaseDir" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\exthost" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\builtin-themes" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ui\frontend\dist" | Out-Null

Write-Host "Copying Vrutti executable..."
if (Test-Path "build\vrutti.exe") {
    Copy-Item "build\vrutti.exe" "$ReleaseDir\vrutti.exe"
} elseif (Test-Path "build\vrutti_app.exe") {
    Copy-Item "build\vrutti_app.exe" "$ReleaseDir\vrutti.exe"
} else {
    Write-Host "ERROR: build\vrutti.exe not found! Compile the project first."
    exit 1
}

Write-Host "Copying plugin DLLs..."
foreach ($dll in Get-ChildItem "build\*.dll") {
    Copy-Item $dll.FullName "$ReleaseDir\"
    Write-Host "  Copied: $($dll.Name)"
}

Write-Host "Copying Node backend (src\ext)..."
Copy-Item "src\ext\*.js" "$ReleaseDir\src\ext\"
if (Test-Path "src\ext\bin\node.exe") {
    Copy-Item "src\ext\bin\node.exe" "$ReleaseDir\src\ext\bin\"
}
if (Test-Path "src\ext\builtin-themes") {
    Copy-Item "src\ext\builtin-themes\*" "$ReleaseDir\src\ext\builtin-themes\" -Recurse
}

# The extension host is bundled so that the release does not need a
# TypeScript loader and no node_modules directory at runtime.
Write-Host "Building the extension host bundle..."
$node = "node"
if (Test-Path "src\ext\bin\node.exe") { $node = "src\ext\bin\node.exe" }
# esbuild writes its progress to stderr. PowerShell turns that into an error
# record, and with `$ErrorActionPreference = "Stop"` it would stop the script,
# so the build runs with both streams redirected to files.
$logDir = ".tmp-storage"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$outLog = Join-Path $logDir "build_exthost.log"
$errLog = Join-Path $logDir "build_exthost.err.log"
Remove-Item $outLog, $errLog -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $node -ArgumentList "scripts\build_exthost.js" `
    -NoNewWindow -Wait -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog
$exitCode = $process.ExitCode
if (Test-Path $outLog) { Get-Content $outLog | Write-Host }
if (Test-Path $errLog) { Get-Content $errLog | Write-Host }
if ($exitCode -ne 0) {
    Write-Host "ERROR: the extension host bundle could not be built."
    exit 1
}
if (Test-Path "src\ext\exthost\extensionHostProcess.mjs") {
    Copy-Item "src\ext\exthost\extensionHostProcess.mjs" "$ReleaseDir\src\ext\exthost\"
    Copy-Item "src\ext\exthost\*.node" "$ReleaseDir\src\ext\exthost\" -ErrorAction SilentlyContinue
    Write-Host "  Copied: extensionHostProcess.mjs"
} else {
    Write-Host "ERROR: src\ext\exthost\extensionHostProcess.mjs not found after the build."
    exit 1
}

Write-Host "Copying the license and the third party notices..."
Copy-Item "LICENSE" "$ReleaseDir\LICENSE"
if (Test-Path "THIRD_PARTY_NOTICES.md") {
    Copy-Item "THIRD_PARTY_NOTICES.md" "$ReleaseDir\THIRD_PARTY_NOTICES.md"
}

# The search plugin runs ripgrep. The binary travels with the installation,
# because the installation directory is not on the PATH of the process.
Write-Host "Copying ripgrep..."
if (Test-Path "bin\rg.exe") {
    Copy-Item "bin\rg.exe" "$ReleaseDir\rg.exe"
    Write-Host "  Copied: rg.exe"
} else {
    Write-Host "WARNING: bin\rg.exe not found. File search will not work."
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
    Write-Host "Done! Installer created at Vrutti_Alpha_0.0.4_SleepyRabbit_Setup.exe"
} else {
    Write-Host "WARNING: Inno Setup compiler (ISCC.exe) not found!"
    Write-Host "The release folder was built, but the installer executable was not created."
    Write-Host "Please download and install Inno Setup from https://jrsoftware.org/isdl.php"
    Write-Host "Then run this script again."
}
