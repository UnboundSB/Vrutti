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
        await ipcClient.sendRequest('host/ready');

        ipcClient.on('extensions/install', async (params) => {
            console.log(`[Bootstrapper] Installing extension ${params.name}...`);
            const https = require('https');
            const fs = require('fs');
            const path = require('path');
            const os = require('os');
            const AdmZip = require('adm-zip');

            const extDir = path.join(os.homedir(), '.vrutti', 'extensions', params.name);
            fs.mkdirSync(extDir, { recursive: true });
            
            const zipPath = path.join(extDir, 'extension.vsix');
            
            const download = (url, dest) => new Promise((resolve, reject) => {
                const req = https.get(url, (res) => {
                    if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                        resolve(download(res.headers.location, dest));
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
                                    ipcClient.sendNotification('extensions/progress', { name: params.name, percentage });
                                }
                            }
                        });

                        res.pipe(file);
                        file.on('finish', () => { 
                            ipcClient.sendNotification('extensions/progress', { name: params.name, percentage: 100 });
                            file.close(); 
                            resolve(); 
                        });
                    } else {
                        reject(new Error(`Failed with status ${res.statusCode}`));
                    }
                });
                req.on('error', reject);
            });

            try {
                await download(params.url, zipPath);
                console.log(`[Bootstrapper] Downloaded to ${zipPath}`);
                
                const zip = new AdmZip(zipPath);
                zip.extractAllTo(extDir, true);
                
                const pkgPath = path.join(extDir, 'extension', 'package.json');
                if (fs.existsSync(pkgPath)) {
                    const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
                    if (pkg.contributes && pkg.contributes.themes && pkg.contributes.themes.length > 0) {
                        const theme = pkg.contributes.themes[0];
                        const themePath = path.join(extDir, 'extension', theme.path);
                        if (fs.existsSync(themePath)) {
                            let themeRaw = fs.readFileSync(themePath, 'utf8');
                            // Strip comments (VS Code themes often have them)
                            themeRaw = themeRaw.replace(/\\/\\*([\\s\\S]*?)\\*\\/|([^\\\\:]|^)\\/\\/.*$/gm, '$2');
                            
                            const themeData = JSON.parse(themeRaw);
                            
                            ipcClient.sendNotification('theme/apply', {
                                name: theme.label || pkg.name,
                                type: theme.uiTheme,
                                colors: themeData.colors
                            });
                            console.log(`[Bootstrapper] Sent theme ${theme.label} to Core`);
                        }
                    }
                }
            } catch (err) {
                console.error(`[Bootstrapper] Install failed:`, err);
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
