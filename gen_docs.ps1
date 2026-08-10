$directories = @(
    "src", "src\app", "src\core", "src\ext", "src\plugins", "src\ui", 
    "src\core\concurrency", "src\core\config", "src\core\editor", "src\core\events", "src\core\fs", 
    "src\core\ipc", "src\core\memory", "src\core\plugins", "src\core\terminal", "src\core\utils", 
    "src\plugins\search", "src\ui\compositor", "src\ui\frontend", "src\ui\frontend\public", 
    "src\ui\frontend\src", "src\ui\frontend\src\components", "src\ui\frontend\src\components\explorer"
)

foreach ($dir in $directories) {
    $fullPath = Join-Path "D:\vrutti\vrutti_ide" $dir
    if (!(Test-Path $fullPath)) { continue }
    
    $structureFile = Join-Path $fullPath "structure.md"
    
    $folderName = Split-Path $dir -Leaf
    if ($folderName -eq "") { $folderName = "src" }
    
    $content = "# Directory: $folderName`n`n## Purpose`nCore module encapsulating $folderName functionality and logic.`n`n"
    
    $subfolders = Get-ChildItem -Path $fullPath -Directory | Where-Object { $_.Name -notmatch 'node_modules|build|\.git|vendor|dist' }
    if ($subfolders) {
        $content += "## Child Directories`n"
        foreach ($sub in $subfolders) {
            $content += "- **" + $sub.Name + "/**: Manages " + $sub.Name + " related abstractions and sub-modules.`n"
        }
        $content += "`n"
    }
    
    $files = Get-ChildItem -Path $fullPath -File | Where-Object { $_.Name -ne 'structure.md' }
    if ($files) {
        $content += "## Files`n"
        foreach ($f in $files) {
            $content += "- **" + $f.Name + "**: Implements internal " + $f.Name + " mechanisms.`n"
        }
    }
    
    Set-Content -Path $structureFile -Value $content
    
    git add $structureFile
    git commit -m "Docs: Update structure.md for $folderName"
}
git push
