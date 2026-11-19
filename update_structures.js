const fs = require('fs');
const path = require('path');

function getDirectories(srcpath) {
    return fs.readdirSync(srcpath, { withFileTypes: true })
        .filter(dirent => dirent.isDirectory())
        .map(dirent => dirent.name);
}

function getFiles(srcpath) {
    return fs.readdirSync(srcpath, { withFileTypes: true })
        .filter(dirent => dirent.isFile() && dirent.name !== 'structure.md')
        .map(dirent => dirent.name);
}

function updateStructureMd(dirPath) {
    const mdPath = path.join(dirPath, 'structure.md');
    if (!fs.existsSync(mdPath)) return;

    const actualFiles = getFiles(dirPath);
    const actualDirs = getDirectories(dirPath);
    
    let content = fs.readFileSync(mdPath, 'utf8');
    
    // Extract existing descriptions
    const fileDesc = {};
    const dirDesc = {};
    
    const lines = content.split('\n');
    let currentSection = '';
    
    for (const line of lines) {
        if (line.startsWith('## Files')) {
            currentSection = 'files';
        } else if (line.startsWith('## Child Directories')) {
            currentSection = 'dirs';
        } else if (line.startsWith('## ')) {
            currentSection = 'other';
        } else if (line.trim().startsWith('- **') && line.includes('**:')) {
            const match = line.trim().match(/^- \*\*([^]+?)\*\*:(.*)/);
            if (match) {
                if (currentSection === 'files') {
                    fileDesc[match[1]] = match[2].trim();
                } else if (currentSection === 'dirs') {
                    dirDesc[match[1]] = match[2].trim();
                }
            }
        }
    }
    
    // Reconstruct sections
    let newFilesSection = '## Files\n';
    if (actualFiles.length === 0) {
        newFilesSection += '- (No files)\n';
    } else {
        for (const file of actualFiles) {
            const desc = fileDesc[file] || 'No description provided.';
            newFilesSection += `- **${file}**: ${desc}\n`;
        }
    }
    
    let newDirsSection = '## Child Directories\n';
    if (actualDirs.length === 0) {
        newDirsSection += '- (No immediate subdirectories)\n';
    } else {
        for (const dir of actualDirs) {
            const desc = dirDesc[dir] || 'No description provided.';
            newDirsSection += `- **${dir}**: ${desc}\n`;
        }
    }
    
    // Replace in content
    const filesRegex = /## Files[\s\S]*?(?=## Child Directories|##|$)/;
    const dirsRegex = /## Child Directories[\s\S]*?(?=## |$)/;
    
    if (filesRegex.test(content)) {
        content = content.replace(filesRegex, newFilesSection + '\n');
    } else {
        content += '\n' + newFilesSection + '\n';
    }
    
    if (dirsRegex.test(content)) {
        content = content.replace(dirsRegex, newDirsSection + '\n');
    } else {
        content += '\n' + newDirsSection + '\n';
    }
    
    fs.writeFileSync(mdPath, content.trim() + '\n', 'utf8');
    console.log(`Updated ${mdPath}`);
}

function traverse(dir) {
    if (fs.existsSync(path.join(dir, 'structure.md'))) {
        updateStructureMd(dir);
    }
    
    const dirs = getDirectories(dir);
    for (const d of dirs) {
        // Skip .git and node_modules
        if (d === '.git' || d === 'node_modules' || d === '.agents' || d === 'Vrutti_Release') continue;
        traverse(path.join(dir, d));
    }
}

const root = path.resolve(process.cwd());
traverse(root);
