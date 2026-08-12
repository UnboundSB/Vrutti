const IPCClient = require('./ipc');
const { createApi } = require('./api');
const Module = require('module');
const fs = require('fs');
const path = require('path');
const os = require('os');

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

function log(msg) {
    console.log(`[Bootstrapper] ${msg}`);
}

class ExtensionManager {
    constructor(ipcClient, api) {
        this.ipcClient = ipcClient;
        this.api = api;
        this.extDirBase = path.join(os.homedir(), '.vrutti', 'extensions');
        if (!fs.existsSync(this.extDirBase)) {
            fs.mkdirSync(this.extDirBase, { recursive: true });
        }
        this._cachedThemes = null;
        this._installedExtensionsCache = null;
        
        // Map of command ID -> { extensionPath: string, main: string }
        this.commandIndex = new Map();
        // Set of active extension IDs
        this.activeExtensions = new Set();
    }

    async indexExtensions() {
        this.commandIndex.clear();
        const extensions = await this.getInstalledExtensions();
        
        for (const ext of extensions) {
            if (ext.main && ext.contributes && ext.contributes.commands) {
                // Determine if we need lazy loading via activationEvents
                // or if we just blindly activate on commands (VS Code does onCommand:id)
                for (const cmd of ext.contributes.commands) {
                    if (cmd.command) {
                        this.commandIndex.set(cmd.command, {
                            id: ext.id,
                            path: ext.localPath,
                            main: path.join(ext.localPath, 'extension', ext.main)
                        });
                    }
                }
            }
        }
    }

    async activateExtensionForCommand(commandId) {
        if (this.commandIndex.has(commandId)) {
            const extInfo = this.commandIndex.get(commandId);
            await this.activateExtension(extInfo);
        }
    }

    async activateExtension(extInfo) {
        if (this.activeExtensions.has(extInfo.id)) {
            return; // Already active
        }
        log(`Lazy activating extension: ${extInfo.id}`);
        try {
            if (fs.existsSync(extInfo.main)) {
                const extModule = require(extInfo.main);
                if (extModule && typeof extModule.activate === 'function') {
                    const context = {
                        subscriptions: [],
                        extensionPath: extInfo.path,
                        globalState: { get: () => undefined, update: () => {} }
                    };
                    await extModule.activate(context);
                    this.activeExtensions.add(extInfo.id);
                    log(`Successfully activated ${extInfo.id}`);
                } else {
                    this.activeExtensions.add(extInfo.id); // Mark as active anyway so we don't retry loop
                }
            }
        } catch (err) {
            log(`Failed to activate extension ${extInfo.id}: ${err.message}`);
            this.activeExtensions.add(extInfo.id);
        }
    }

    async getInstalledExtensions() {
        if (this._installedExtensionsCache) return this._installedExtensionsCache;

        const installed = [];
        if (!fs.existsSync(this.extDirBase)) return installed;
        
        const dirs = await fs.promises.readdir(this.extDirBase);
        for (const dir of dirs) {
            const extPath = path.join(this.extDirBase, dir);
            const pkgPath = path.join(extPath, 'extension', 'package.json');
            if (fs.existsSync(pkgPath)) {
                try {
                    const pkgRaw = await fs.promises.readFile(pkgPath, 'utf8');
                    const pkg = JSON.parse(pkgRaw);
                    installed.push({
                        id: `${pkg.publisher || pkg.author || dir}.${pkg.name}`,
                        name: pkg.name,
                        displayName: pkg.displayName || pkg.name,
                        publisherDisplayName: pkg.publisher || pkg.author || dir,
                        description: pkg.description || '',
                        version: pkg.version || '1.0.0',
                        isTheme: pkg.contributes && pkg.contributes.themes ? true : false,
                        localPath: extPath,
                        main: pkg.main,
                        contributes: pkg.contributes,
                        activationEvents: pkg.activationEvents
                    });
                    
                    // Register TextMate grammars
                    const { registerGrammars } = require('./textmate-engine');
                    registerGrammars(extPath, pkg.contributes);
                    
                } catch (e) {
                    console.error(`Failed to read package.json for ${dir}`);
                }
            }
        }
        this._installedExtensionsCache = installed;
        return installed;
    }

    async getAvailableThemes() {
        if (this._cachedThemes) return this._cachedThemes;
        const themes = [];
        const pathsToScan = [
            { dir: path.join(__dirname, 'builtin-themes'), isBuiltin: true },
            { dir: this.extDirBase, isBuiltin: false }
        ];

        for (const target of pathsToScan) {
            if (!fs.existsSync(target.dir)) continue;
            
            if (target.isBuiltin) {
                const pkgPath = path.join(target.dir, 'package.json');
                if (fs.existsSync(pkgPath)) {
                    try {
                        const pkgRaw = await fs.promises.readFile(pkgPath, 'utf8');
                        const pkg = JSON.parse(pkgRaw);
                        if (pkg.contributes && pkg.contributes.themes) {
                            for (const theme of pkg.contributes.themes) {
                                const origPath = path.join(target.dir, theme.path);
                                themes.push({
                                    id: theme.id || theme.label,
                                    label: theme.label || pkg.name,
                                    uiTheme: theme.uiTheme || 'vs-dark',
                                    extensionName: pkg.name,
                                    themePath: origPath
                                });
                            }
                        }
                    } catch (e) {}
                }
            } else {
                const dirs = await fs.promises.readdir(target.dir);
                for (const dir of dirs) {
                    const extPath = path.join(target.dir, dir);
                    const pkgPath = path.join(extPath, 'extension', 'package.json');
                    if (fs.existsSync(pkgPath)) {
                        try {
                            const pkgRaw = await fs.promises.readFile(pkgPath, 'utf8');
                            const pkg = JSON.parse(pkgRaw);
                            if (pkg.contributes && pkg.contributes.themes) {
                                for (const theme of pkg.contributes.themes) {
                                    const origPath = path.join(extPath, 'extension', theme.path);
                                    themes.push({
                                        id: `${pkg.name}.${theme.id || theme.label}`,
                                        label: theme.label || pkg.name,
                                        uiTheme: theme.uiTheme || 'vs-dark',
                                        extensionName: pkg.name,
                                        themePath: origPath
                                    });
                                }
                            }
                        } catch (e) {}
                    }
                }
            }
        }
        this._cachedThemes = themes;
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
        await new Promise((resolve, reject) => {
            zip.extractAllToAsync(extDir, true, false, (error) => {
                if (error) reject(error); else resolve();
            });
        });
        
        await fs.promises.unlink(zipPath);
        log(`Successfully installed extension ${name}`);
        await this.invalidateCache();
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

    async invalidateCache() {
        this._cachedThemes = null;
        this._installedExtensionsCache = null;
        await this.indexExtensions();
    }

    translateThemeColorsInMemory(themeJson) {
        const tokenMap = {
            'editor.background': '--vrutti-bg',
            'sideBar.background': '--vrutti-surface',
            'activityBar.background': '--vrutti-surface',
            'editorGroupHeader.tabsBackground': '--vrutti-surface',
            'editor.foreground': '--vrutti-text-bright',
            'sideBarTitle.foreground': '--vrutti-text',
            'tab.activeBackground': '--vrutti-surface-border',
            'button.background': '--vrutti-accent',
            'focusBorder': '--vrutti-accent',
            'editorLineNumber.foreground': '--vrutti-text',
            'terminal.background': '--vrutti-bg',
            'gitDecoration.modifiedResourceForeground': '--vrutti-git-modified',
            'gitDecoration.untrackedResourceForeground': '--vrutti-git-untracked',
            'gitDecoration.deletedResourceForeground': '--vrutti-git-deleted'
        };

        const result = { colors: {}, tokenColors: themeJson.tokenColors || [] };
        if (themeJson.colors) {
            for (const [vsToken, colorValue] of Object.entries(themeJson.colors)) {
                if (tokenMap[vsToken]) {
                    result.colors[tokenMap[vsToken]] = colorValue;
                } else {
                    // Forward unknown colors directly too, UI may or may not use them
                    result.colors[vsToken] = colorValue;
                }
            }
        }
        // Basic fallback
        if (!result.colors['--vrutti-bg']) {
            result.colors['--vrutti-bg'] = themeJson.type === 'light' ? '#ffffff' : '#1e1e1e';
        }

        return result;
    }
}

async function main() {
    const config = parseArgs();
    
    if (!config.pipeName) {
        console.error('Error: --pipe argument is required');
        process.exit(1);
    }

    log(`Starting Extension Host with pipe: ${config.pipeName}`);

    const ipcClient = new IPCClient(config.pipeName);
    
    try {
        await ipcClient.connect();
        log('Connected to native C++ engine.');
        
        const vruttiApi = createApi(ipcClient);
        
        // Inject the module into Node's module resolution cache
        // Allow BOTH 'vscode' and 'vrutti' to ensure we support standard extensions
        // while not breaking internal ones.
        const originalRequire = Module.prototype.require;
        Module.prototype.require = function(id) {
            if (id === 'vscode' || id === 'vrutti') {
                return vruttiApi;
            }
            return originalRequire.apply(this, arguments);
        };
        
        ipcClient.sendNotification('host/ready');
        log('Bootstrapper started');

        const manager = new ExtensionManager(ipcClient, vruttiApi);
        await manager.indexExtensions();
        
        ipcClient.on('extensions/request_installed', async () => {
            const installedExts = await manager.getInstalledExtensions();
            ipcClient.sendNotification('extensions/installed', installedExts);
            ipcClient.sendNotification('themes/available', await manager.getAvailableThemes());
        });

        ipcClient.on('extensions/uninstall', async (params) => {
            log(`Uninstalling extension ${params.name}`);
            try {
                const extDir = path.join(manager.extDirBase, params.name);
                if (fs.existsSync(extDir)) {
                    await fs.promises.rm(extDir, { recursive: true, force: true });
                }
                await manager.invalidateCache();
                ipcClient.sendNotification('extensions/installed', await manager.getInstalledExtensions());
                ipcClient.sendNotification('themes/available', await manager.getAvailableThemes());
            } catch (err) {
                log(`Failed to uninstall extension ${params.name}: ${err.message}`);
            }
        });

        ipcClient.on('extensions/install', async (params) => {
            log(`Installing extension ${params.name} from ${params.url}`);
            try {
                await manager.downloadExtension(params.url, params.name, (percentage) => {
                    ipcClient.sendNotification('extensions/progress', { name: params.name, percentage });
                });
                ipcClient.sendNotification('extensions/progress', { name: params.name, percentage: 100 });
                ipcClient.sendNotification('extensions/installed', await manager.getInstalledExtensions());
                ipcClient.sendNotification('themes/available', await manager.getAvailableThemes());
            } catch (err) {
                log(`Failed to install extension ${params.name}: ${err.message}\n${err.stack}`);
            }
        });

        ipcClient.on('theme/set', async (params) => {
            const themeLabelOrExtName = params.name;
            log(`Setting theme to ${themeLabelOrExtName}`);
            try {
                const themes = await manager.getAvailableThemes();
                let targetTheme = themes.find(t => t.id === params.id) || 
                                  themes.find(t => t.id === themeLabelOrExtName) ||
                                  themes.find(t => t.label === themeLabelOrExtName) || 
                                  themes.find(t => t.extensionName === themeLabelOrExtName);
                
                if (targetTheme && fs.existsSync(targetTheme.themePath)) {
                    try {
                        const { loadThemeRecursive } = require('./theme-resolver');
                        const originalThemeData = await loadThemeRecursive(targetTheme.themePath);
                        const translatedThemeData = manager.translateThemeColorsInMemory(originalThemeData);
                        
                        log(`Loading theme: ${targetTheme.label}`);
                        translatedThemeData.name = targetTheme.label;
                        translatedThemeData.id = targetTheme.id;
                        translatedThemeData.uiTheme = targetTheme.uiTheme;
                        
                        ipcClient.sendNotification('theme/load', translatedThemeData);
                    } catch (e) {
                        console.error(`Failed to parse theme JSON: ${targetTheme.themePath}`, e);
                    }
                } else {
                    log(`Theme not found or missing path for ${themeLabelOrExtName}`);
                }
            } catch (err) {
                log(`Failed to set theme ${themeLabelOrExtName}: ${err.message}`);
            }
        });

        // Register default internal command
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
            
            if (userParams) cmdString += ` ${userParams}`;
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

                await vruttiApi.commands.executeCommand('vrutti.action.run', file, mode, userParams);
            } catch (err) {
                log(`Failed to execute run command: ${err.message}`);
                ipcClient.sendNotification('run/output', { text: `Error: ${err.message}\n` });
            }
        });
        
        ipcClient.on('editor/tokenize', async (payloadJson) => {
            try {
                const req = typeof payloadJson === 'string' ? JSON.parse(payloadJson) : payloadJson;
                const { textmateEngine } = require('./textmate-engine'); // lazy load
                // wait, I exported it directly
                const { tokenizeDocument } = require('./textmate-engine');
                const tokens = await tokenizeDocument(req.languageId, req.text);
                if (tokens) {
                    ipcClient.sendNotification('editor/tokens', { fileId: req.fileId, tokens });
                }
            } catch (e) {
                log(`Tokenization error: ${e.message}`);
            }
        });
        
        // Intercept generic command execution from UI
        ipcClient.on('command/execute', async (payload) => {
            try {
                const commandId = payload.command;
                const args = payload.args || [];
                // 1. Ensure extension is loaded
                await manager.activateExtensionForCommand(commandId);
                // 2. Execute
                await vruttiApi.commands.executeCommand(commandId, ...args);
            } catch (err) {
                log(`Command execution failed: ${err.message}`);
            }
        });
        
        if (config.extensionPath) {
            log(`Loading extension from: ${config.extensionPath}`);
            try {
                const extModule = require(config.extensionPath);
                if (extModule && typeof extModule.activate === 'function') {
                    await extModule.activate({
                        subscriptions: [],
                        extensionPath: config.extensionPath,
                        globalState: { get: () => undefined, update: () => {} }
                    });
                }
            } catch (err) {
                console.error(`Failed to load extension:`, err);
            }
        }
    } catch (err) {
        console.error('[Bootstrapper] Initialization failed:', err);
        process.exit(1);
    }
}

main();
