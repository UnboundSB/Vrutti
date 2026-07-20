const vrutti = require('vrutti');

async function activate() {
    console.log('[MockExtension] Activation started.');

    try {
        // Test Workspace API over IPC
        console.log('[MockExtension] Requesting root path...');
        const rootPath = await vrutti.workspace.getRootPath();
        console.log(`[MockExtension] Root path received via IPC: ${rootPath}`);

        // Test Window API over IPC
        console.log('[MockExtension] Sending information message to native UI...');
        await vrutti.window.showInformationMessage('Hello from the Mock Extension over IPC!');
        
        console.log('[MockExtension] Extension activated and tested successfully!');
    } catch (err) {
        console.error('[MockExtension] IPC communication test failed:', err);
    }
}

module.exports = {
    activate
};
