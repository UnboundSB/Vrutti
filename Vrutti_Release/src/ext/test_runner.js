const fs = require('fs');
const path = require('path');
const https = require('https');
const AdmZip = require('adm-zip');
const { createApi } = require('./legacy/api');
const Module = require('module');

const EXTENSIONS_TO_TEST = [
    { publisher: 'esbenp', name: 'prettier-vscode', version: '11.0.0' },
    { publisher: 'dbaeumer', name: 'vscode-eslint', version: '3.0.10' },
    { publisher: 'eamodio', name: 'gitlens', version: '15.2.2' }
];

const TEST_DIR = path.join(__dirname, 'test_exts');

const { execSync } = require('child_process');

function downloadVsix(publisher, name, version, outPath) {
    const url = `https://open-vsx.org/api/${publisher}/${name}/${version}/file/${publisher}.${name}-${version}.vsix`;
    console.log(`[TEST] Downloading ${url} ...`);
    const curlCmd = process.platform === 'win32' ? 'curl.exe' : 'curl';
    try {
        execSync(`${curlCmd} -sL --compressed "${url}" -o "${outPath}"`);
        return Promise.resolve(outPath);
    } catch (e) {
        return Promise.reject(e);
    }
}

// Mock IPC Client
class MockIPCClient {
    sendRequest() { return Promise.resolve({}); }
    sendNotification() {}
    on() {}
    removeListener() {}
}

async function runTests() {
    console.log('[TEST] Setting up Vrutti API Proxy Mock...');
    
    const ipcClient = new MockIPCClient();
    const vruttiApi = createApi(ipcClient);

    const originalRequire = Module.prototype.require;
    Module.prototype.require = function(id) {
        if (id === 'vscode' || id === 'vrutti') {
            return vruttiApi;
        }
        return originalRequire.apply(this, arguments);
    };

    if (!fs.existsSync(TEST_DIR)) {
        fs.mkdirSync(TEST_DIR, { recursive: true });
    }

    for (const ext of EXTENSIONS_TO_TEST) {
        console.log(`\n[TEST] ======= Testing ${ext.name} =======`);
        const extDir = path.join(TEST_DIR, ext.name);
        if (!fs.existsSync(extDir)) {
            const vsixPath = path.join(TEST_DIR, `${ext.name}.vsix`);
            console.log(`[TEST] Downloading ${ext.name}...`);
            try {
                await downloadVsix(ext.publisher, ext.name, ext.version, vsixPath);
                console.log(`[TEST] Extracting ${ext.name}...`);
                const zip = new AdmZip(vsixPath);
                zip.extractAllTo(extDir, true);
                fs.unlinkSync(vsixPath);
            } catch(e) {
                console.error(`[TEST] Failed to download or extract ${ext.name}:`, e.message);
                continue;
            }
        }

        const pkgPath = path.join(extDir, 'extension', 'package.json');
        if (!fs.existsSync(pkgPath)) {
            console.error(`[TEST] Missing package.json for ${ext.name}`);
            continue;
        }

        const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf8'));
        if (!pkg.main) {
            console.log(`[TEST] No main entry point for ${ext.name}, skipping activation.`);
            continue;
        }

        const mainPath = path.join(extDir, 'extension', pkg.main);
        console.log(`[TEST] Activating ${ext.name} at ${mainPath}...`);
        
        try {
            const extModule = originalRequire.call(module, mainPath);
            if (extModule && typeof extModule.activate === 'function') {
                const context = {
                    subscriptions: [],
                    extensionPath: path.join(extDir, 'extension'),
                    globalState: { 
                        get: () => undefined, 
                        update: () => {},
                        setKeysForSync: () => {}
                    },
                    workspaceState: {
                        get: () => undefined, 
                        update: () => {}
                    },
                    secrets: {
                        get: async () => undefined,
                        store: async () => {},
                        delete: async () => {},
                        onDidChange: () => ({ dispose: () => {} })
                    },
                    extension: {
                        packageJSON: pkg
                    },
                    asAbsolutePath: (p) => path.join(path.join(extDir, 'extension'), p)
                };
                // Wrap context in a proxy so missing properties don't crash the extension
                const proxiedContext = new Proxy(context, {
                    get: (obj, prop) => {
                        if (prop in obj) return obj[prop];
                        if (typeof prop === 'symbol') return undefined;
                        console.warn(`[Vrutti API Stub] Called unimplemented context property: ${String(prop)}`);
                        return function(...args) {
                            console.warn(`[Vrutti API Stub] Called unimplemented context method: ${String(prop)}`);
                            return { dispose: () => {} };
                        };
                    }
                });

                const activationPromise = extModule.activate(proxiedContext);
                if (activationPromise instanceof Promise) {
                    await activationPromise;
                }
                
                console.log(`[SUCCESS] ${ext.name} activated without crashing!`);
            } else {
                console.log(`[WARNING] ${ext.name} has no activate function.`);
            }
        } catch (err) {
            console.error(`[FAIL] ${ext.name} crashed during activation!`);
            console.error(err.stack);
        }
    }
}

runTests().catch(console.error);
