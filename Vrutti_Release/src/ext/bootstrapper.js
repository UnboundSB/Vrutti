const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

// Vrutti IDE bootstrapper that acts as the "renderer" main process
// and spawns the official VS Code Extension Host.

const pipeArg = process.argv.find(arg => arg.startsWith('--pipe='));
if (!pipeArg) {
    console.error('[Bootstrapper] Error: --pipe argument missing.');
    process.exit(1);
}

const pipeName = pipeArg.split('=')[1];

// Find the VS Code extension host process
// Try compiled 'out' directory first, fallback to 'src' with tsx
const vscodeRoot = path.resolve(__dirname, '../../../');
let extHostPath = path.join(vscodeRoot, 'out/vs/workbench/api/node/extensionHostProcess.js');
let useTsx = false;

if (!fs.existsSync(extHostPath)) {
    console.log('[Bootstrapper] Compiled extHost not found, falling back to TS source...');
    // Vrutti renamed 'vs' to 'vr'
    extHostPath = path.join(vscodeRoot, 'src/vr/workbench/api/node/extensionHostProcess.ts');
    useTsx = true;
}

console.log(`[Bootstrapper] Launching Extension Host: ${extHostPath}`);

const env = {
    ...process.env,
    // Provide the pipe for VS Code's IPC
    VSCODE_EXTHOST_IPC_HOOK: pipeName,
    VSCODE_EXTHOST_IPC_HOOK_EXTHOST: pipeName,
    VSCODE_HANDLES_UNCAUGHT_ERRORS: true
};

let spawnCmd = process.execPath;
const args = ['--require', path.join(__dirname, 'crypto-polyfill.js'), '--expose-gc'];
if (useTsx) {
    const tsxPath = path.resolve(__dirname, '../../node_modules/tsx/dist/cli.mjs');
    args.push(tsxPath, '--tsconfig=' + path.join(__dirname, '../../tsconfig.exthost.json'), extHostPath);
} else {
    args.push(extHostPath);
}

// Spawn the extension host
const child = spawn(spawnCmd, args, {
    env,
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
});

child.stdout.on('data', (data) => {
    console.log(`[ExtHost] ${data}`);
});

child.stderr.on('data', (data) => {
    console.error(`[ExtHost ERR] ${data}`);
});

child.on('message', (msg) => {
    if (msg.type === 'VSCODE_EXTHOST_IPC_READY') {
        console.log('[Bootstrapper] ExtHost is ready for socket connection!');
        // In the socket model, we'd pass the socket via message
        // But since we are using a pipe, we might not need to if ExtHost connects directly.
    }
});

child.on('exit', (code) => {
    console.log(`[Bootstrapper] ExtHost exited with code ${code}`);
    process.exit(code);
});
