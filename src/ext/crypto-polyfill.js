// Preload script for the VS Code Extension Host process.
//
// It runs before any extension host module is evaluated and does two things:
//
//  1. Provides `crypto.webcrypto` on runtimes that do not expose it.
//  2. Injects Vrutti's product/package configuration, so the extension host
//     identifies itself as Vrutti IDE rather than "Code - OSS Dev".
//     (`src/vr/platform/product/common/product.ts` reads these globals.)

if (!globalThis.crypto) {
    try {
        globalThis.crypto = require('crypto').webcrypto;
    } catch (e) {
        // Fallback: leave `crypto` undefined.
    }
}

(function injectProductConfiguration() {
    if (globalThis._VSCODE_PRODUCT_JSON) {
        return; // already injected by an outer launcher
    }

    const path = require('path');
    const fs = require('fs');

    // src/ext -> repository root
    const repoRoot = path.resolve(__dirname, '../..');

    const productPath = process.env.VRUTTI_PRODUCT_JSON ||
        path.join(repoRoot, 'product.json');
    const packagePath = process.env.VRUTTI_PACKAGE_JSON ||
        path.join(repoRoot, 'package.json');

    try {
        if (fs.existsSync(productPath)) {
            globalThis._VSCODE_PRODUCT_JSON = JSON.parse(fs.readFileSync(productPath, 'utf8'));
        }
    } catch (e) {
        console.error('[Bootstrapper] Failed to read product.json:', e.message);
    }

    try {
        if (fs.existsSync(packagePath)) {
            globalThis._VSCODE_PACKAGE_JSON = JSON.parse(fs.readFileSync(packagePath, 'utf8'));
        }
    } catch (e) {
        console.error('[Bootstrapper] Failed to read package.json:', e.message);
    }
})();
