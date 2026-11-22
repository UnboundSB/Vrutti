$ErrorActionPreference = "Stop"

$ReleaseDir = "Vrutti_Release"
$ZipFile = "vrutti_early_release.zip"

Write-Host "Cleaning up old release folder..."
if (Test-Path $ReleaseDir) { Remove-Item -Recurse -Force $ReleaseDir }
if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }

Write-Host "Creating release structure..."
New-Item -ItemType Directory -Force -Path "$ReleaseDir" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\bin" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\exthost" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ext\builtin-themes" | Out-Null
New-Item -ItemType Directory -Force -Path "$ReleaseDir\src\ui\frontend\dist" | Out-Null

Write-Host "Copying Vrutti executable..."
# CMake writes the executable as `vrutti.exe`. The old name is kept as a
# fallback so that an older build directory still packages.
if (Test-Path "build\vrutti.exe") {
    Copy-Item "build\vrutti.exe" "$ReleaseDir\vrutti.exe"
} elseif (Test-Path "build\vrutti_app.exe") {
    Copy-Item "build\vrutti_app.exe" "$ReleaseDir\vrutti.exe"
} else {
    Write-Host "ERROR: build\vrutti.exe not found! Compile the project first."
    exit 1
}

Write-Host "Copying plugin DLLs..."
# The search plugin is loaded on demand, the rest sit next to the executable.
if (Test-Path "build") {
    foreach ($dll in Get-ChildItem "build\*.dll") {
        Copy-Item $dll.FullName "$ReleaseDir\"
        Write-Host "  Copied: $($dll.Name)"
    }
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
    Copy-Item "src\ui\frontend\dist" "$ReleaseDir\src\ui\frontend" -Recurse -Force
} else {
    Write-Host "ERROR: src\ui\frontend\dist not found! Run frontend build first."
    exit 1
}

Write-Host "Compressing to $ZipFile..."
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipFile

Write-Host "Done! Release package created at $ZipFile"
