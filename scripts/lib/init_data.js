// Builds the `IExtensionHostInitData` payload and `IExtensionDescription`
// objects that Vrutti sends to the VS Code Extension Host.
//
// The shapes here MUST mirror:
//   * src/core/plugins/VSCodeExtHostService.cpp  (init data)
//   * src/core/plugins/ExtensionManager.cpp       (extension descriptions)
//   * src/vr/workbench/services/extensions/common/extensionHostProtocol.ts
//
// The tests use this module so that the Node-side harness and the C++ core
// agree on the contract; `scripts/test_init_data_parity.js` asserts that the
// C++ implementation produces byte-for-byte equivalent JSON.

'use strict';

const path = require('path');
const fs = require('fs');
const crypto = require('crypto');
const { fileUri } = require('./exthost_harness');

/**
 * Deterministic UUID (MD5 based, RFC 4122 v3 style) so extension identity is
 * stable across restarts, exactly like `ExtensionManager::stableUuid`.
 */
function stableUuid(seed) {
    const hash = crypto.createHash('md5').update(seed, 'utf8').digest('hex');
    return [
        hash.slice(0, 8),
        hash.slice(8, 12),
        hash.slice(12, 16),
        hash.slice(16, 20),
        hash.slice(20, 32),
    ].join('-');
}

/**
 * Builds an `IExtensionDescription` from an extension's `package.json`.
 *
 * @param {string} packageJsonPath absolute path to the manifest
 * @param {string} extensionRoot    absolute path of the extension folder
 *                                   (the folder `main`/`browser` are relative to)
 */
function describeExtension(packageJsonPath, extensionRoot) {
    const pkg = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));

    const publisher = pkg.publisher || 'unknown';
    const name = pkg.name || path.basename(path.dirname(packageJsonPath));
    const id = `${publisher}.${name}`;

    const identifier = {
        value: id,
        _lower: id.toLowerCase(),
        id,
    };

    const desc = {
        identifier,
        uuid: stableUuid(id),
        name,
        publisher,
        publisherDisplayName: publisher,
        version: pkg.version || '0.0.0',
        engines: (pkg.engines && pkg.engines.vscode) ? { vscode: pkg.engines.vscode } : '^1.90.0',
        extensionLocation: fileUri(extensionRoot),
        targetPlatform: 'undefined',
        isBuiltin: false,
        isUserBuiltin: false,
        isUnderDevelopment: false,
        preRelease: false,
    };

    if (pkg.displayName) desc.displayName = pkg.displayName;
    if (pkg.main) desc.main = pkg.main;
    if (pkg.browser) desc.browser = pkg.browser;
    if (Array.isArray(pkg.activationEvents)) desc.activationEvents = pkg.activationEvents;
    if (pkg.contributes) desc.contributes = pkg.contributes;
    if (pkg.extensionKind) desc.extensionKind = pkg.extensionKind;
    if (pkg.capabilities) desc.capabilities = pkg.capabilities;

    return desc;
}

/**
 * Builds the `IExtensionHostInitData` payload.
 *
 * @param {object} options
 * @param {string} options.repoRoot
 * @param {string} [options.workspaceFolder]
 * @param {Array<object>} [options.extensions] IExtensionDescription list
 */
function buildInitData(options) {
    const repoRoot = options.repoRoot;
    const workspaceFolder = options.workspaceFolder || repoRoot;
    const extensions = options.extensions || [];

    const allExtensions = extensions;
    const activationEvents = {};
    const myExtensions = [];

    for (const desc of extensions) {
        const id = desc.identifier.value;
        activationEvents[id] = desc.activationEvents || [];
        myExtensions.push({ value: desc.identifier.value, _lower: desc.identifier._lower });
    }

    const storage = path.join(repoRoot, '.tmp-storage');

    return {
        version: '1.90.0',
        quality: 'stable',
        commit: 'vrutti',
        date: '2024-01-01',
        parentPid: process.pid,
        environment: {
            isExtensionDevelopmentDebug: false,
            appName: 'Vrutti IDE',
            appHost: 'desktop',
            appRoot: fileUri(repoRoot),
            appLanguage: 'en',
            isExtensionTelemetryLoggingOnly: false,
            appUriScheme: 'vrutti',
            globalStorageHome: fileUri(path.join(storage, 'global')),
            workspaceStorageHome: fileUri(path.join(storage, 'workspace')),
        },
        workspace: {
            id: 'vrutti-workspace',
            name: path.basename(workspaceFolder),
            configuration: null,
            isUntitled: false,
        },
        extensions: {
            versionId: 1,
            allExtensions,
            activationEvents,
            myExtensions,
        },
        telemetryInfo: {
            sessionId: 'vrutti-session',
            machineId: 'vrutti-machine',
            sqmId: 'vrutti-sqm',
            devDeviceId: 'vrutti-device',
            firstSessionDate: '2024-01-01',
        },
        logLevel: 1,
        loggers: [],
        logsLocation: fileUri(path.join(storage, 'logs')),
        autoStart: true,
        remote: { isRemote: false, authority: null, connectionData: null },
        consoleForward: { includeStack: false, logNative: false },
        uiKind: 1,
    };
}

/**
 * Builds the `IWorkspaceData` payload for `ExtHostWorkspace.$initializeWorkspace`.
 */
function buildWorkspaceData(workspaceFolder) {
    const name = path.basename(workspaceFolder);
    return {
        id: name.toLowerCase().replace(/[^a-z0-9.-]/g, '') || 'vrutti-workspace',
        name,
        folders: [
            { uri: fileUri(workspaceFolder), name, index: 0 },
        ],
    };
}

/**
 * Builds the `IConfigurationInitData` payload for
 * `ExtHostConfiguration.$initializeConfiguration`.
 *
 * The extension host keeps a barrier in front of its configuration provider:
 * `vscode.workspace.getConfiguration()` blocks until the renderer has sent
 * this payload.  Without it any extension that reads a setting hangs forever
 * during activation.
 *
 * Shape: `IConfigurationData` + `configurationScopes`.  Every model is an
 * `IConfigurationModel`, i.e. `{ contents, keys, overrides }` -- an empty
 * object is *not* a valid model and makes the host throw while parsing.
 */
function buildConfigurationInitData(options = {}) {
    const models = options.models || {};
    const emptyModel = () => ({ contents: {}, keys: [], overrides: [] });
    const model = (name) => (models[name] ? models[name] : emptyModel());
    return {
        defaults: model('defaults'),
        policy: model('policy'),
        application: model('application'),
        userLocal: model('userLocal'),
        userRemote: model('userRemote'),
        workspace: model('workspace'),
        folders: options.folders || [],
        configurationScopes: options.configurationScopes || [],
    };
}

module.exports = {
    describeExtension,
    buildInitData,
    buildWorkspaceData,
    buildConfigurationInitData,
    stableUuid,
};
