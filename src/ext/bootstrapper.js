const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

/**
 * Vrutti IDE bootstrapper.
 *
 * Acts as the bridge between the native C++ core and the *official* VS Code
 * Extension Host. The extension host is what implements the `vscode.*` API for
 * extensions -- Vrutti does not re-implement it, it runs the real thing.
 *
 * The extension host is expected to speak the "VruttiJSONBridge" protocol over
 * the named pipe handed to us via --pipe=<name>.
 *
 * ENTRY POINT DISCOVERY (first match wins)
 * -----------------------------------------
 *   1. $VRUTTI_EXTHOST_ENTRY          explicit override
 *   2. <ext>/exthost/...js            bundled build shipped with Vrutti
 *   3. <ext>/exthost/...ts            Vrutti's own entry point, run through tsx
 *   4. <vscodeRoot>/out/vr/...js      compiled upstream output
 *   5. <vscodeRoot>/src/vr/...ts      upstream sources, run through tsx
 *
 * Vrutti ships its own entry point (3) because the upstream bridge has a
 * byte-collision bug that prevents every extension from activating. See
 * `exthost/extensionHostProcess.ts` for the details.
 *
 * VS CODE ROOT DISCOVERY
 * ----------------------
 *   1. $VRUTTI_VSCODE_ROOT
 *   2. <ext>/../../..            (development checkout layout)
 *   3. <ext>/../../../..         (release nested one level deeper)
 */

const pipeArg = process.argv.find(arg => arg.startsWith('--pipe='));
if (!pipeArg) {
    console.error('[Bootstrapper] FATAL: --pipe=<name> argument is missing.');
    process.exit(1);
}
const pipeName = pipeArg.split('=').slice(1).join('=');

const extDir = __dirname;

// --- locate the VS Code source tree ------------------------------------------
const vscodeRootCandidates = [
    process.env.VRUTTI_VSCODE_ROOT,
    path.resolve(extDir, '../../..'),
    path.resolve(extDir, '../../../..'),
].filter(Boolean);

const vscodeRoot = vscodeRootCandidates.find(root => {
    try {
        return fs.existsSync(path.join(root, 'src', 'vr', 'workbench', 'api', 'node', 'extensionHostProcess.ts'))
            || fs.existsSync(path.join(root, 'src', 'vs', 'workbench', 'api', 'node', 'extensionHostProcess.ts'));
    } catch (_) {
        return false;
    }
});

// --- locate the extension host entry point ------------------------------------
function exists(p) {
    try { return !!p && fs.existsSync(p); } catch (_) { return false; }
}

function findTsx() {
    const candidates = [
        path.resolve(extDir, '../../node_modules/tsx/dist/cli.mjs'),
        path.resolve(extDir, '../../../node_modules/tsx/dist/cli.mjs'),
        path.resolve(extDir, '../../node_modules/tsx/dist/loader.mjs'),
    ];
    if (vscodeRoot) {
        candidates.push(path.resolve(vscodeRoot, 'node_modules/tsx/dist/cli.mjs'));
    }
    return candidates.find(exists);
}

function resolveEntryPoint() {
    // 1. explicit override
    if (process.env.VRUTTI_EXTHOST_ENTRY && exists(process.env.VRUTTI_EXTHOST_ENTRY)) {
        return { file: process.env.VRUTTI_EXTHOST_ENTRY, useTsx: false };
    }

    // 2. bundled build shipped alongside this file
    const bundled = path.resolve(extDir, 'exthost', 'extensionHostProcess.js');
    if (exists(bundled)) {
        return { file: bundled, useTsx: false };
    }

    // 3. Vrutti's own entry point (TypeScript, needs the VS Code tree)
    const ownEntry = path.resolve(extDir, 'exthost', 'extensionHostProcess.ts');
    if (exists(ownEntry)) {
        const tsx = findTsx();
        if (tsx) {
            return { file: ownEntry, useTsx: true, tsx };
        }
    }

    if (vscodeRoot) {
        // 4. compiled upstream output (the tree renames 'vs' to 'vr')
        const compiled = [
            path.join(vscodeRoot, 'out', 'vr', 'workbench', 'api', 'node', 'extensionHostProcess.js'),
            path.join(vscodeRoot, 'out', 'vs', 'workbench', 'api', 'node', 'extensionHostProcess.js'),
        ].find(exists);
        if (compiled) {
            return { file: compiled, useTsx: false };
        }

        // 5. upstream TypeScript sources through tsx
        const tsSource = [
            path.join(vscodeRoot, 'src', 'vr', 'workbench', 'api', 'node', 'extensionHostProcess.ts'),
            path.join(vscodeRoot, 'src', 'vs', 'workbench', 'api', 'node', 'extensionHostProcess.ts'),
        ].find(exists);
        if (tsSource) {
            const tsx = findTsx();
            if (tsx) {
                return { file: tsSource, useTsx: true, tsx };
            }
        }
    }

    return null;
}

const entry = resolveEntryPoint();

if (!entry) {
    console.error('[Bootstrapper] FATAL: could not locate the VS Code Extension Host.');
    console.error('[Bootstrapper] Searched:');
    console.error(`  - $VRUTTI_EXTHOST_ENTRY (${process.env.VRUTTI_EXTHOST_ENTRY || 'unset'})`);
    console.error(`  - ${path.resolve(extDir, 'exthost', 'extensionHostProcess.js')}`);
    if (vscodeRoot) {
        console.error(`  - ${path.join(vscodeRoot, 'out', 'vr', 'workbench', 'api', 'node', 'extensionHostProcess.js')}`);
        console.error(`  - ${path.join(vscodeRoot, 'src', 'vr', 'workbench', 'api', 'node', 'extensionHostProcess.ts')} (via tsx)`);
    } else {
        console.error(`  - VS Code root was not found (checked ${vscodeRootCandidates.join(', ')})`);
    }
    console.error('[Bootstrapper] The extension host is unavailable; extensions will not load.');
    process.exit(1);
}

console.log(`[Bootstrapper] VS Code root:  ${vscodeRoot || '(not found)'}`);
console.log(`[Bootstrapper] Extension Host: ${entry.file}${entry.useTsx ? ' (via tsx)' : ''}`);
console.log(`[Bootstrapper] Pipe hook:  ${pipeName}`);

// --- build the child command line ---------------------------------------------
const env = {
    ...process.env,
    // The extension host reads this to learn which pipe to connect to.
    VSCODE_EXTHOST_IPC_HOOK: pipeName,
    VSCODE_HANDLES_UNCAUGHT_ERRORS: true,
};

// Preloads must be injected through NODE_OPTIONS: the TypeScript loader (tsx)
// spawns the *real* extension host process as a child, and that child does not
// inherit the parent's `--require` flags.
const preloads = [path.join(extDir, 'crypto-polyfill.js')];
if (process.env.VRUTTI_TRACE_BRIDGE) {
    const tracePreload = path.resolve(extDir, '../../scripts/lib/trace_preload.js');
    if (exists(tracePreload)) {
        preloads.push(tracePreload);
    }
}
const existingNodeOptions = env.NODE_OPTIONS ? env.NODE_OPTIONS.trim() : '';
env.NODE_OPTIONS = [
    ...preloads.map((p) => `--require ${JSON.stringify(p)}`),
    existingNodeOptions,
].filter(Boolean).join(' ');

const args = ['--expose-gc'];
if (entry.useTsx) {
    const tsconfig = path.resolve(extDir, '../../tsconfig.exthost.json');
    args.push(entry.tsx);
    if (exists(tsconfig)) {
        args.push('--tsconfig=' + tsconfig);
    }
    args.push(entry.file);
} else {
    args.push(entry.file);
}

console.log(`[Bootstrapper] Spawning: ${process.execPath} ${args.join(' ')}`);

const child = spawn(process.execPath, args, {
    env,
    stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
    cwd: vscodeRoot || extDir,
});

child.stdout.on('data', (data) => {
    console.log(`[ExtHost] ${data}`);
});

child.stderr.on('data', (data) => {
    console.error(`[ExtHost ERR] ${data}`);
});

child.on('message', (msg) => {
    if (msg && msg.type === 'VSCODE_EXTHOST_IPC_READY') {
        console.log('[Bootstrapper] Extension Host signalled IPC_READY.');
    }
});

child.on('error', (err) => {
    console.error(`[Bootstrapper] FATAL: failed to spawn the extension host: ${err.message}`);
    process.exit(1);
});

child.on('exit', (code, signal) => {
    if (signal) {
        console.error(`[Bootstrapper] Extension Host terminated by signal ${signal}.`);
    } else {
        console.log(`[Bootstrapper] Extension Host exited with code ${code}.`);
    }
    process.exit(code === null ? 1 : code);
});

// Keep the IPC channel open for the lifetime of the child.
process.on('disconnect', () => {
    console.error('[Bootstrapper] IPC channel to the native core closed.');
});
