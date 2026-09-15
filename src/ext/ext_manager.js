const fs = require('fs');
const path = require('path');
const os = require('os');
const https = require('https');
const AdmZip = require('adm-zip');

const COMMAND = process.argv[2];

const EXT_DIR_BASE = path.join(os.homedir(), '.vrutti', 'extensions');
if (!fs.existsSync(EXT_DIR_BASE)) {
    fs.mkdirSync(EXT_DIR_BASE, { recursive: true });
}

function log(msg) {
    console.error(`[ExtManager] ${msg}`);
}

function downloadFile(url, dest, progressCallback) {
    return new Promise((resolve, reject) => {
        const options = {
            headers: {
                'User-Agent': 'Vrutti-IDE/1.0'
            }
        };
        const req = https.get(url, options, (res) => {
            if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                let redirectUrl = res.headers.location;
                if (!redirectUrl.startsWith('http')) {
                    const parsedUrl = new URL(url);
                    redirectUrl = `${parsedUrl.protocol}//${parsedUrl.host}${redirectUrl.startsWith('/') ? '' : '/'}${redirectUrl}`;
                }
                resolve(downloadFile(redirectUrl, dest, progressCallback));
            } else if (res.statusCode === 200) {
                const totalSize = parseInt(res.headers['content-length'] || '0', 10);
                let downloaded = 0;
                let lastPercentage = -1;

                const file = fs.createWriteStream(dest);
                res.on('data', (chunk) => {
                    downloaded += chunk.length;
                    if (totalSize > 0) {
                        const percentage = Math.round((downloaded / totalSize) * 100);
                        if (percentage !== lastPercentage) {
                            lastPercentage = percentage;
                            if (progressCallback) progressCallback(percentage);
                        }
                    }
                });

                res.pipe(file);
                file.on('finish', () => { 
                    file.close(); 
                    resolve(); 
                });
                file.on('error', reject);
            } else {
                reject(new Error(`Failed with status ${res.statusCode}`));
            }
        });
        req.on('error', reject);
    });
}

async function installExtension(url, name) {
    log(`Installing extension ${name} from ${url}`);
    const extDir = path.join(EXT_DIR_BASE, name);
    if (!fs.existsSync(extDir)) {
        fs.mkdirSync(extDir, { recursive: true });
    }
    
    const zipPath = path.join(extDir, 'extension.vsix');
    
    try {
        await downloadFile(url, zipPath, (percentage) => {
            // Write to stdout specifically so C++ can parse it easily
            console.log(JSON.stringify({ type: 'progress', percentage }));
        });
        
        log(`Extracting ${zipPath}...`);
        const zip = new AdmZip(zipPath);
        await new Promise((resolve, reject) => {
            zip.extractAllToAsync(extDir, true, false, (error) => {
                if (error) reject(error); else resolve();
            });
        });
        
        await fs.promises.unlink(zipPath);
        log(`Successfully installed extension ${name}`);
        console.log(JSON.stringify({ type: 'success' }));
    } catch (e) {
        log(`Failed to install extension: ${e.message}`);
        console.log(JSON.stringify({ type: 'error', message: e.message }));
        process.exit(1);
    }
}

async function uninstallExtension(name) {
    log(`Uninstalling extension ${name}`);
    const extDir = path.join(EXT_DIR_BASE, name);
    if (fs.existsSync(extDir)) {
        try {
            await fs.promises.rm(extDir, { recursive: true, force: true });
            log(`Successfully uninstalled extension ${name}`);
            console.log(JSON.stringify({ type: 'success' }));
        } catch (e) {
            log(`Failed to uninstall extension: ${e.message}`);
            console.log(JSON.stringify({ type: 'error', message: e.message }));
            process.exit(1);
        }
    } else {
        console.log(JSON.stringify({ type: 'success' })); // Already uninstalled
    }
}

const BUILTIN_EXT_DIR = 'D:\\vrutti\\extensions';

async function listExtensions() {
    const installed = [];
    
    async function scanDir(baseDir, isBuiltin) {
        if (!fs.existsSync(baseDir)) return;
        const dirs = await fs.promises.readdir(baseDir);
        for (const dir of dirs) {
            const extPath = path.join(baseDir, dir);
            let pkgPath = path.join(extPath, 'extension', 'package.json');
            
            // For built-ins, the package.json is usually directly in the extPath, not in 'extension' subdirectory
            if (!fs.existsSync(pkgPath)) {
                pkgPath = path.join(extPath, 'package.json');
            }
            
            if (fs.existsSync(pkgPath)) {
            try {
                const pkgRaw = await fs.promises.readFile(pkgPath, 'utf8');
                const pkg = JSON.parse(pkgRaw);
                let nls = null;
                const nlsPath = path.join(extPath, 'extension', 'package.nls.json');
                if (fs.existsSync(nlsPath)) {
                    try {
                        const nlsRaw = await fs.promises.readFile(nlsPath, 'utf8');
                        nls = JSON.parse(nlsRaw);
                    } catch (e) {}
                }

                const safeNls = nls || {};
                const localize = (obj) => {
                    if (typeof obj === 'string') {
                        return obj.replace(/%([^%]+)%/g, (match, key) => {
                            if (safeNls[key] !== undefined) return safeNls[key];
                            if (safeNls[match] !== undefined) return safeNls[match];
                            const stripped = key.replace(/^extension\./, '');
                            if (safeNls[stripped] !== undefined) return safeNls[stripped];
                            
                            if (key.includes('displayName')) return pkg.name;
                            if (key.includes('description')) return '';
                            return match;
                        });
                    } else if (Array.isArray(obj)) {
                        return obj.map(localize);
                    } else if (obj && typeof obj === 'object') {
                        for (const k in obj) {
                            obj[k] = localize(obj[k]);
                        }
                    }
                    return obj;
                };
                localize(pkg);

                localize(pkg);

                let fileUriPath = extPath.replace(/\\/g, '/');
                if (!fileUriPath.startsWith('/')) {
                    fileUriPath = '/' + fileUriPath;
                }
                
                installed.push({
                    id: `${pkg.publisher || pkg.author || dir}.${pkg.name}`,
                    identifier: { value: `${pkg.publisher || pkg.author || dir}.${pkg.name}`, _lower: `${pkg.publisher || pkg.author || dir}.${pkg.name}`.toLowerCase() },
                    name: pkg.name,
                    displayName: pkg.displayName || pkg.name,
                    publisherDisplayName: pkg.publisher || pkg.author || dir,
                    publisher: pkg.publisher || pkg.author || dir,
                    description: pkg.description || '',
                    version: pkg.version || '1.0.0',
                    isTheme: pkg.contributes && (pkg.contributes.themes || pkg.contributes.iconThemes) ? true : false,
                    extensionLocation: {
                        $mid: 1,
                        path: fileUriPath,
                        scheme: 'file'
                    },
                    localPath: extPath,
                    main: pkg.main,
                    browser: pkg.browser,
                    contributes: pkg.contributes,
                    activationEvents: pkg.activationEvents,
                    engines: pkg.engines || { vscode: "^1.90.0" },
                    targetPlatform: 'universal',
                    isBuiltin: isBuiltin,
                    isUserBuiltin: isBuiltin,
                    isUnderDevelopment: false,
                    preRelease: false
                });
            } catch (e) {
                log(`Failed to read package.json for ${dir}: ${e.message}`);
            }
        }
    }
    }

    await scanDir(BUILTIN_EXT_DIR, true);
    await scanDir(EXT_DIR_BASE, false);
    
    // Output just the JSON array to stdout
    console.log(JSON.stringify(installed));
}

async function main() {
    if (COMMAND === 'install') {
        const url = process.argv[3];
        const name = process.argv[4];
        if (!url || !name) {
            log('Usage: install <url> <name>');
            process.exit(1);
        }
        await installExtension(url, name);
    } else if (COMMAND === 'uninstall') {
        const name = process.argv[3];
        if (!name) {
            log('Usage: uninstall <name>');
            process.exit(1);
        }
        await uninstallExtension(name);
    } else if (COMMAND === 'list') {
        await listExtensions();
    } else {
        log('Unknown command. Use install, uninstall, or list.');
        process.exit(1);
    }
}

main().catch(e => {
    log(`Fatal error: ${e.message}`);
    process.exit(1);
});
