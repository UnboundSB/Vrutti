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

            getAvailableThemes() {
                const themes = [];
                if (!fs.existsSync(this.extDirBase)) return themes;
                
                const dirs = fs.readdirSync(this.extDirBase);
                for (const dir of dirs) {
                    const extPath = path.join(this.extDirBase, dir);
                    const pkgPath = path.join(extPath, 'extension', 'package.json');
                    if (fs.existsSync(pkgPath)) {
                        try {
                            const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
                            if (pkg.contributes && pkg.contributes.themes) {
                                for (const theme of pkg.contributes.themes) {
                                    themes.push({
                                        id: `${pkg.name}.${theme.id || theme.label}`,
                                        label: theme.label || pkg.name,
                                        uiTheme: theme.uiTheme || 'vs-dark',
                                        extensionName: pkg.name,
                                        themePath: path.join(extPath, 'extension', theme.path)
                                    });
                                }
                            }
                        } catch (e) {}
                    }
                }
                return themes;
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
                                
                                try {
                                    const themeData = JSON.parse(themeRaw);
                                    
                                    // Forward to C++ core
                                    log(`Loading theme: ${theme.label}`);
                                    ipcClient.sendNotification('theme/load', themeData);
                                } catch (e) {
                                    console.error(`Failed to parse theme JSON: ${themePath}`);
                                }
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
        
        ipcClient.on('extensions/request_installed', () => {
            const installedExts = downloader.getInstalledExtensions();
            ipcClient.sendNotification('extensions/installed', installedExts);
            ipcClient.sendNotification('themes/available', downloader.getAvailableThemes());
        });

        ipcClient.on('extensions/uninstall', (params) => {
            log(`Uninstalling extension ${params.name}`);
            try {
                const extDir = path.join(downloader.extDirBase, params.name);
                if (fs.existsSync(extDir)) {
                    fs.rmSync(extDir, { recursive: true, force: true });
                }
                ipcClient.sendNotification('extensions/installed', downloader.getInstalledExtensions());
                ipcClient.sendNotification('themes/available', downloader.getAvailableThemes());
            } catch (err) {
                log(`Failed to uninstall extension ${params.name}: ${err.message}`);
            }
        });

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
                ipcClient.sendNotification('themes/available', downloader.getAvailableThemes());
            } catch (err) {
                log(`Failed to install extension ${params.name}: ${err.message}\n${err.stack}`);
            }
        });
        ipcClient.on('theme/set', (params) => {
            const themeLabelOrExtName = params.name; // Could be extension name (from extensions list) or specific theme label
            log(`Setting theme to ${themeLabelOrExtName}`);
            try {
                const themes = downloader.getAvailableThemes();
                let targetTheme = themes.find(t => t.id === params.id) || 
                                  themes.find(t => t.label === themeLabelOrExtName) || 
                                  themes.find(t => t.extensionName === themeLabelOrExtName);
                
                if (targetTheme && fs.existsSync(targetTheme.themePath)) {
                    let themeRaw = fs.readFileSync(targetTheme.themePath, 'utf8');
                    themeRaw = themeRaw.replace(/\/\*([\s\S]*?)\*\/|([^\\:]|^)\/\/.*$/gm, '$2');
                    try {
                        const themeData = JSON.parse(themeRaw);
                        log(`Loading theme: ${targetTheme.label}`);
                        // Pass along the name so the UI knows which theme was loaded
                        themeData.name = targetTheme.label;
                        themeData.id = targetTheme.id;
                        ipcClient.sendNotification('theme/load', themeData);
                    } catch (e) {
                        console.error(`Failed to parse theme JSON: ${targetTheme.themePath}`);
                    }
                } else {
                    log(`Theme not found or missing path for ${themeLabelOrExtName}`);
                }
            } catch (err) {
                log(`Failed to set theme ${themeLabelOrExtName}: ${err.message}`);
            }
        });

        // Built-in runner extension
        vruttiApi.commands.registerCommand('vrutti.action.run', async (file, mode, userParams) => {
            const url = require('url');
            if (file.startsWith('file://')) {
                file = url.fileURLToPath(file);
            }

            const ext = path.extname(file);
            const dir = path.dirname(file);
            const baseName = path.basename(file, ext);
            
            let cmdString = '';
            
            if (ext === '.py') {
                cmdString = `python "${file}"`;
            } else if (ext === '.js') {
                cmdString = mode === 'debug' ? `node --inspect "${file}"` : `node "${file}"`;
            } else if (ext === '.ts') {
                cmdString = mode === 'debug' ? `npx ts-node --inspect "${file}"` : `npx ts-node "${file}"`;
            } else if (ext === '.cpp' || ext === '.c') {
                const isWin = os.platform() === 'win32';
                const exeName = isWin ? `${baseName}.exe` : baseName;
                const outPath = path.join(dir, exeName);
                const compiler = ext === '.cpp' ? 'g++' : 'gcc';
                
                cmdString = `${compiler} "${file}" -o "${outPath}" && "${outPath}"`;
            } else if (ext === '.html' || ext === '.htm') {
                const isWin = os.platform() === 'win32';
                cmdString = isWin ? `start "" "${file}"` : (os.platform() === 'darwin' ? `open "${file}"` : `xdg-open "${file}"`);
            } else {
                ipcClient.sendNotification('run/output', { text: `Unsupported file extension: ${ext}\n` });
                return;
            }
            
            if (userParams) {
                cmdString += ` ${userParams}`;
            }
            
            ipcClient.sendNotification('terminal/runCommand', { command: cmdString });
        });

        ipcClient.on('editor/run', async (payloadJson) => {
            try {
                const req = typeof payloadJson === 'string' ? JSON.parse(payloadJson) : payloadJson;
                if (!req || typeof req !== 'object') return;
                
                const file = req.file;
                const mode = req.mode || 'run';
                const userParams = req.params || '';
                
                if (!file) return;

                // Dispatch to the extension command
                await vruttiApi.commands.executeCommand('vrutti.action.run', file, mode, userParams);

            } catch (err) {
                log(`Failed to execute run command: ${err.message}`);
                ipcClient.sendNotification('run/output', { text: `Error: ${err.message}\n` });
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
