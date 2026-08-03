const IPCClient = require('./ipc');
const { createApi } = require('./api');
const Module = require('module');

function parseArgs() {
    const args = process.argv.slice(2);
    const config = {};
    for (const arg of args) {
        if (arg.startsWith('--pipe=')) {
            config.pipeName = arg.split('=')[1];
        } else if (arg.startsWith('--ext=')) {
            config.extensionPath = arg.split('=')[1];
        }
    }
    return config;
}

async function main() {
    const config = parseArgs();
    
    if (!config.pipeName) {
        console.error('Error: --pipe argument is required');
        process.exit(1);
    }

    console.log(`[Bootstrapper] Starting Extension Host with pipe: ${config.pipeName}`);

    const ipcClient = new IPCClient(config.pipeName);
    
    try {
        await ipcClient.connect();
        console.log('[Bootstrapper] Connected to native C++ engine.');
        
        const vruttiApi = createApi(ipcClient);
        
        // Inject the 'vrutti' module into Node's module resolution cache
        // This allows extensions to safely do: const vrutti = require('vrutti');
        const originalRequire = Module.prototype.require;
        Module.prototype.require = function(id) {
            if (id === 'vrutti') {
                return vruttiApi;
            }
            return originalRequire.apply(this, arguments);
        };
        
        // Let the C++ core know we are ready to receive commands
        ipcClient.sendNotification('host/ready');

        const fs = require('fs');
        const path = require('path');
        const os = require('os');

        function log(msg) {
            console.log(`[Bootstrapper] ${msg}`);
        }

        log('Bootstrapper started');

        class ExtensionDownloader {
            constructor() {
                this.extDirBase = path.join(os.homedir(), '.vrutti', 'extensions');
                if (!fs.existsSync(this.extDirBase)) {
                    fs.mkdirSync(this.extDirBase, { recursive: true });
                }
            }

            getInstalledExtensions() {
                const installed = [];
                if (!fs.existsSync(this.extDirBase)) return installed;
                
                const dirs = fs.readdirSync(this.extDirBase);
                for (const dir of dirs) {
                    const extPath = path.join(this.extDirBase, dir);
                    const pkgPath = path.join(extPath, 'extension', 'package.json');
                    if (fs.existsSync(pkgPath)) {
                        try {
                            const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
                            installed.push({
                                name: pkg.name,
                                displayName: pkg.displayName || pkg.name,
                                publisherDisplayName: pkg.publisher || pkg.author || dir,
                                description: pkg.description || '',
                                version: pkg.version || '1.0.0',
                                isTheme: pkg.contributes && pkg.contributes.themes ? true : false,
                                localPath: extPath
                            });
                        } catch (e) {
                            console.error(`Failed to read package.json for ${dir}`);
                        }
                    }
                }
                return installed;
            }

            async downloadExtension(url, name, progressCallback) {
                const extDir = path.join(this.extDirBase, name);
                if (!fs.existsSync(extDir)) fs.mkdirSync(extDir, { recursive: true });
                
                const zipPath = path.join(extDir, 'extension.vsix');
                
                await this._downloadFile(url, zipPath, progressCallback);
                
                log(`Extracting ${zipPath}...`);
                const AdmZip = require('adm-zip');
                const zip = new AdmZip(zipPath);
                zip.extractAllTo(extDir, true);
                
                // Special handling for themes
                const pkgJsonPath = path.join(extDir, 'extension', 'package.json');
                if (fs.existsSync(pkgJsonPath)) {
                    const pkg = JSON.parse(fs.readFileSync(pkgJsonPath, 'utf8'));
                    if (pkg.contributes && pkg.contributes.themes) {
                        for (const theme of pkg.contributes.themes) {
                            const themePath = path.join(extDir, 'extension', theme.path);
                            if (fs.existsSync(themePath)) {
                                let themeRaw = fs.readFileSync(themePath, 'utf8');
                                // Strip comments (VS Code themes often have them)
                                themeRaw = themeRaw.replace(/\/\*([\s\S]*?)\*\/|([^\\:]|^)\/\/.*$/gm, '$2');
                                
                                const themeData = JSON.parse(themeRaw);
                                
                                // Forward to C++ core
                                log(`Loading theme: ${theme.label}`);
                                ipcClient.sendNotification('theme/load', themeData);
                            }
                        }
                    }
                }
                
                fs.unlinkSync(zipPath);
                log(`Successfully installed extension ${name}`);
            }

            _downloadFile(url, dest, progressCallback) {
                const https = require('https');
                return new Promise((resolve, reject) => {
                    const req = https.get(url, (res) => {
                        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                            resolve(this._downloadFile(res.headers.location, dest, progressCallback));
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
        }

        const downloader = new ExtensionDownloader();
        
        // Broadcast installed extensions to frontend on startup
        const installedExts = downloader.getInstalledExtensions();
        ipcClient.sendNotification('extensions/installed', installedExts);

        ipcClient.on('extensions/install', async (params) => {
            log(`Installing extension ${params.name} from ${params.url}`);
            try {
                await downloader.downloadExtension(params.url, params.name, (percentage) => {
                    ipcClient.sendNotification('extensions/progress', { name: params.name, percentage });
                });
                // Ensure UI knows we hit 100%
                ipcClient.sendNotification('extensions/progress', { name: params.name, percentage: 100 });
                // Broadcast updated list
                ipcClient.sendNotification('extensions/installed', downloader.getInstalledExtensions());
            } catch (err) {
                log(`Failed to install extension ${params.name}: ${err.message}\n${err.stack}`);
            }
        });
        
        if (config.extensionPath) {
            console.log(`[Bootstrapper] Loading extension from: ${config.extensionPath}`);
            try {
                const extModule = require(config.extensionPath);
                if (extModule && typeof extModule.activate === 'function') {
                    await extModule.activate();
                }
            } catch (err) {
                console.error(`[Bootstrapper] Failed to load extension:`, err);
            }
        }
    } catch (err) {
        console.error('[Bootstrapper] Initialization failed:', err);
        process.exit(1);
    }
}

main();
