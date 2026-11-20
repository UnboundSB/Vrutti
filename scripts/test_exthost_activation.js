// End-to-end activation test for the Vrutti <-> VS Code Extension Host link.
//
// It reproduces, in Node, exactly what Vrutti's native C++ core does:
//
//   1. the host connects and says "ready"
//   2. the core sends the init payload (IExtensionHostInitData)
//   3. the core calls ExtHostWorkspace.$initializeWorkspace(workspace, trusted)
//      -- this opens the barrier the host waits on during initialize()
//   4. the core calls ExtHostExtensionService.$activateByEvent("onStartupFinished")
//   5. the host loads the extension, the extension activates, and the host
//      calls back into the renderer (MainThreadCommands.$registerCommand)
//
// A real extension is installed for the duration of the test
// (src/ext/test_exts/vrutti-hello), so a pass means extensions genuinely
// activate -- not merely that a handshake completed.
//
// Usage:
//   node scripts/test_exthost_activation.js [timeoutMs]

'use strict';

const path = require('path');
const fs = require('fs');

const { ExtHostHarness, REPO_ROOT } = require('./lib/exthost_harness');
const {
    describeExtension,
    buildInitData,
    buildWorkspaceData,
    buildConfigurationInitData,
} = require('./lib/init_data');

const TIMEOUT_MS = Number(process.argv[2] || 90000);

const TEST_EXTENSION_DIR = path.join(REPO_ROOT, 'src', 'ext', 'test_exts', 'vrutti-hello');
const TEST_EXTENSION_ROOT = path.join(TEST_EXTENSION_DIR, 'extension');
const TEST_EXTENSION_PKG = path.join(TEST_EXTENSION_ROOT, 'package.json');

const EXPECTED_COMMAND = 'vrutti.hello.sayHello';

const failures = [];
function check(condition, message) {
    if (condition) {
        console.log(`  [PASS] ${message}`);
    } else {
        console.error(`  [FAIL] ${message}`);
        failures.push(message);
    }
}

async function main() {
    if (!fs.existsSync(TEST_EXTENSION_PKG)) {
        console.error(`[test] missing test extension manifest: ${TEST_EXTENSION_PKG}`);
        process.exit(1);
    }

    const description = describeExtension(TEST_EXTENSION_PKG, TEST_EXTENSION_ROOT);
    const initData = buildInitData({
        repoRoot: REPO_ROOT,
        workspaceFolder: REPO_ROOT,
        extensions: [description],
    });
    const workspaceData = buildWorkspaceData(REPO_ROOT);
    const configurationData = buildConfigurationInitData();

    console.log(`[test] extension: ${description.identifier.value}`);
    console.log(`[test] activation events: ${JSON.stringify(description.activationEvents)}`);

    const harness = new ExtHostHarness({ timeoutMs: TIMEOUT_MS, verbose: true });

    // --- answer the host's calls into the renderer ----------------------------
    // The renderer side owns these actors. We implement just enough of them
    // for the test extension to activate, and record what was asked for.
    const seen = new Map();

    harness.on('request', (frame) => {
        const shape = shapeNameOf(harness, frame.proxyId);
        const key = `${shape}.${frame.method}`;
        seen.set(key, (seen.get(key) || 0) + 1);

        switch (key) {
            case 'MainThreadCommands.$registerCommand':
                harness.sendReply(frame.reqId, null);
                break;

            case 'MainThreadOutputService.$createOutputChannel':
                harness.sendReply(frame.reqId, { id: 'vrutti-test-channel' });
                break;

            case 'MainThreadExtensionService.$onDidActivateExtension':
                harness.sendReply(frame.reqId, null);
                break;

            case 'MainThreadExtensionService.$onExtensionRuntimeError':
            case 'MainThreadExtensionService.$onExtensionActivationError':
                harness.sendReply(frame.reqId, null);
                break;

            case 'MainThreadErrors.$onUnexpectedError':
                harness.sendReply(frame.reqId, null);
                break;

            case 'MainThreadWorkspace.$getWorkspaceFolderId':
            case 'MainThreadWorkspace.$workspaceFolders':
                harness.sendReply(frame.reqId, null);
                break;

            // The window state is destructured by the host, so it must be an
            // object -- `null` makes the host throw.
            case 'MainThreadWindow.$getInitialState':
                harness.sendReply(frame.reqId, { isFocused: true, isActive: true });
                break;

            // The tool list is iterated by the host, so it must be an array.
            case 'MainThreadLanguageModelTools.$getTools':
                harness.sendReply(frame.reqId, []);
                break;

            default:
                // Unknown calls still get an answer so the extension is not
                // left hanging on a promise.
                harness.sendReply(frame.reqId, null);
                break;
        }
    });

    await harness.start();

    // --- 1. wait for "ready" --------------------------------------------------
    console.log('\n[test] step 1: waiting for the extension host to report ready');
    await harness.waitFor((f) => f.type === 'ready', TIMEOUT_MS, '"ready" frame');
    check(harness.ready, 'extension host reported ready');

    // --- 2. send the init payload --------------------------------------------
    console.log('\n[test] step 2: sending init data');
    harness.sendInit(initData);
    check(true, 'init data sent');

    // --- 3. initialise the configuration ----------------------------------------
    // The extension host keeps a barrier in front of its configuration provider.
    // Any extension that reads a setting blocks until this call happened.
    console.log('\n[test] step 3: ExtHostConfiguration.$initializeConfiguration');
    const configCall = harness.sendShapeRequest(
        'ExtHostConfiguration',
        '$initializeConfiguration',
        [configurationData]
    );
    await configCall.promise;
    check(true, '$initializeConfiguration acknowledged by the extension host');

    // --- 4. initialise the workspace ------------------------------------------
    console.log('\n[test] step 4: ExtHostWorkspace.$initializeWorkspace');
    const wsCall = harness.sendShapeRequest(
        'ExtHostWorkspace',
        '$initializeWorkspace',
        [workspaceData, true]
    );
    await wsCall.promise;
    check(true, '$initializeWorkspace acknowledged by the extension host');

    // --- 5. trigger startup activation ----------------------------------------
    console.log('\n[test] step 5: ExtHostExtensionService.$activateByEvent("onStartupFinished")');
    const activationCall = harness.sendShapeRequest(
        'ExtHostExtensionService',
        '$activateByEvent',
        ['onStartupFinished', 0] // 0 == ActivationKind.Normal
    );
    await activationCall.promise;
    check(true, '$activateByEvent acknowledged by the extension host');

    // --- 6. the extension must register its command ----------------------------
    console.log('\n[test] step 6: waiting for the extension to register its command');
    let registered = false;
    try {
        await harness.waitFor(
            (f) => f.type === 'request'
                && f.proxyId === harness.proxyIds.MainThreadCommands
                && f.method === '$registerCommand'
                && Array.isArray(f.args)
                && f.args[0] === EXPECTED_COMMAND,
            Math.max(5000, TIMEOUT_MS - 5000),
            `MainThreadCommands.$registerCommand("${EXPECTED_COMMAND}")`
        );
        registered = true;
    } catch (e) {
        console.error(`  [FAIL] ${e.message}`);
        failures.push(e.message);
    }
    check(registered, `extension registered the command "${EXPECTED_COMMAND}"`);

    // --- report ----------------------------------------------------------------
    console.log('\n[test] renderer calls observed:');
    for (const [key, count] of seen) {
        console.log(`         ${count}x ${key}`);
    }

    const runtimeErrors = harness.hostRequests.filter(
        (f) => f.method === '$onExtensionRuntimeError' || f.method === '$onExtensionActivationError'
    );
    check(runtimeErrors.length === 0, 'no extension runtime/activation errors were reported');

    harness.sendTerminate();
    await new Promise((r) => setTimeout(r, 500));
    harness.stop();

    console.log('');
    if (failures.length > 0) {
        console.error(`[test] FAILED (${failures.length} assertion(s)):`);
        for (const f of failures) console.error(`         - ${f}`);
        process.exit(1);
    }

    console.log('[test] OK: extension activated through the real VS Code Extension Host');
    process.exit(0);
}

/** Maps a numeric proxyId back to its shape name using the generated table. */
function shapeNameOf(harness, proxyId) {
    for (const [name, id] of Object.entries(harness.proxyIds)) {
        if (id === proxyId) return name;
    }
    return `Unknown(${proxyId})`;
}

main().catch((err) => {
    console.error('[test] FATAL:', err);
    process.exit(1);
});
