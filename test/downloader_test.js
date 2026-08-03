const https = require('https');
const fs = require('fs');
const path = require('path');
const os = require('os');
const AdmZip = require('adm-zip');

class ExtensionDownloader {
    constructor() {
        this.extDirBase = path.join(os.homedir(), '.vrutti', 'extensions');
        if (!fs.existsSync(this.extDirBase)) {
            fs.mkdirSync(this.extDirBase, { recursive: true });
        }
    }

    async downloadExtension(url, name, progressCallback) {
        const extDir = path.join(this.extDirBase, name);
        if (!fs.existsSync(extDir)) fs.mkdirSync(extDir, { recursive: true });
        
        const zipPath = path.join(extDir, 'extension.vsix');
        
        await this._downloadFile(url, zipPath, progressCallback);
        
        console.log(`Extracting ${zipPath}...`);
        const zip = new AdmZip(zipPath);
        zip.extractAllTo(extDir, true);
        
        fs.unlinkSync(zipPath);
        console.log(`Successfully installed ${name}`);
    }

    _downloadFile(url, dest, progressCallback) {
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

// Test runner
async function runTest() {
    console.log('Testing ExtensionDownloader...');
    const downloader = new ExtensionDownloader();
    
    const url = 'https://open-vsx.org/api/PROxZIMA/sweetdracula/1.0.9/file/PROxZIMA.sweetdracula-1.0.9.vsix';
    const name = 'sweetdracula';
    
    try {
        await downloader.downloadExtension(url, name, (pct) => {
            process.stdout.write(`\rProgress: ${pct}%   `);
        });
        console.log('\nTest passed!');
    } catch (e) {
        console.error('\nTest failed:', e);
    }
}

runTest();
