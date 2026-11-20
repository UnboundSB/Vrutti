// Integration test for the Vrutti <-> VS Code Extension Host handshake.
//
// Acts as the native C++ core would: opens the named pipe, launches the
// extension host through the bootstrapper, performs the VruttiJSONBridge
// handshake and reports what the extension host sends back.
//
// Usage:
//   node scripts/test_exthost_handshake.js [timeoutMs]

const net = require('net');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

const TIMEOUT_MS = Number(process.argv[2] || 45000);
const repoRoot = path.resolve(__dirname, '..');
const extDir = path.join(repoRoot, 'src', 'ext');
const pipeName = `\\\\.\\pipe\\vrutti_test_${process.pid}`;

console.log(`[test] pipe: ${pipeName}`);

// --- minimal but structurally valid IExtensionHostInitData --------------------
function fileUri(p) {
    let norm = p.replace(/\\/g, '/');
    if (/^[a-zA-Z]:/.test(norm)) { norm = '/' + norm; }
    return { $mid: 1, scheme: 'file', path: norm };
}

const initData = {
    version: '1.90.0',
    quality: 'stable',
    parentPid: process.pid,
    environment: {
        isExtensionDevelopmentDebug: false,
        appName: 'Vrutti IDE',
        appHost: 'desktop',
        appRoot: fileUri(repoRoot),
        appLanguage: 'en',
        isExtensionTelemetryLoggingOnly: false,
        appUriScheme: 'vrutti',
        globalStorageHome: fileUri(path.join(repoRoot, '.tmp-storage', 'global')),
        workspaceStorageHome: fileUri(path.join(repoRoot, '.tmp-storage', 'workspace')),
    },
    workspace: { id: 'test', name: 'Test', configuration: null, isUntitled: false },
    extensions: {
        versionId: 1,
        allExtensions: [],
        activationEvents: {},
        myExtensions: [],
    },
    telemetryInfo: {
        sessionId: 'test-session',
        machineId: 'test-machine',
        sqmId: 'test-sqm',
        devDeviceId: 'test-device',
        firstSessionDate: '2024-01-01',
    },
    logLevel: 1,
    loggers: [],
    logsLocation: fileUri(path.join(repoRoot, '.tmp-storage', 'logs')),
    autoStart: true,
    remote: { isRemote: false, authority: null, connectionData: null },
    consoleForward: { includeStack: false, logNative: false },
    uiKind: 1,
};

// --- pipe server --------------------------------------------------------------
const server = net.createServer();

let child = null;
let buffer = '';
let gotReady = false;
let sentInit = false;
const received = [];

function finish(code, message) {
    console.log(`[test] ${message}`);
    try { if (child) child.kill(); } catch (_) { /* ignore */ }
    try { server.close(); } catch (_) { /* ignore */ }
    setTimeout(() => process.exit(code), 250);
}

server.on('error', (err) => {
    console.error('[test] pipe server error:', err.message);
    process.exit(1);
});

server.listen(pipeName, () => {
    console.log('[test] pipe listening; launching extension host...');

    const nodeExe = process.execPath;
    const bootstrapper = path.join(extDir, 'bootstrapper.js');
    child = spawn(nodeExe, [bootstrapper, `--pipe=${pipeName}`], {
        cwd: repoRoot,
        stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
        env: { ...process.env },
    });

    child.stdout.on('data', d => process.stdout.write(`[exthost] ${d}`));
    child.stderr.on('data', d => process.stderr.write(`[exthost-err] ${d}`));
    child.on('exit', (code, signal) => {
        console.log(`[test] extension host exited (code=${code}, signal=${signal})`);
    });
});

server.on('connection', (socket) => {
    console.log('[test] extension host connected to the pipe');

    socket.on('data', (data) => {
        buffer += data.toString('utf8');
        let idx;
        while ((idx = buffer.indexOf('\n')) !== -1) {
            const line = buffer.slice(0, idx);
            buffer = buffer.slice(idx + 1);
            if (!line.trim()) { continue; }

            let msg;
            try {
                msg = JSON.parse(line);
            } catch (e) {
                console.log(`[test] non-JSON frame: ${line.slice(0, 200)}`);
                continue;
            }

            console.log(`[test] << ${msg.type || '?'} ${msg.method || ''} ${msg.reqId !== undefined ? 'reqId=' + msg.reqId : ''}`);
            received.push(msg);

            if (msg.type === 'ready' && !gotReady) {
                gotReady = true;
                console.log('[test] >> init');
                socket.write(JSON.stringify({ type: 'init', data: initData }) + '\n');
                sentInit = true;
            }

            if (msg.type === 'request') {
                // Answer so the extension host is not left waiting.
                socket.write(JSON.stringify({ type: 'reply', reqId: msg.reqId, res: null }) + '\n');
                console.log(`[test] >> reply ${msg.reqId}`);
            }
        }
    });

    socket.on('error', (err) => console.error('[test] socket error:', err.message));
    socket.on('close', () => console.log('[test] socket closed'));
});

// --- timeout ------------------------------------------------------------------
setTimeout(() => {
    if (!gotReady) {
        finish(1, 'TIMEOUT: extension host never reported ready');
        return;
    }
    if (!sentInit) {
        finish(1, 'TIMEOUT: init data was never sent');
        return;
    }
    const hasRequest = received.some(m => m.type === 'request');
    console.log(`[test] frames received: ${received.length}, saw RPC request: ${hasRequest}`);
    finish(0, 'OK: handshake completed');
}, TIMEOUT_MS);
