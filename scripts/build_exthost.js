// Builds the Vrutti Extension Host bundle.
//
// The extension host is the MIT licensed service layer of the VS Code source
// tree (see THIRD_PARTY_NOTICES.md).  At development time it can be run
// straight from TypeScript with `tsx`, but a release must not depend on a
// TypeScript loader -- it has to run on the bundled Node.js runtime with a
// plain `.js` file.
//
// This script bundles `src/ext/exthost/extensionHostProcess.ts` and everything
// it pulls in into a single ES module:
//
//     src/ext/exthost/extensionHostProcess.mjs
//
// The output is an ES module and not CommonJS because the service layer uses
// `import.meta.url` and `import.meta.dirname`.  In a CommonJS bundle those
// are empty, and the extension host then cannot resolve the modules of an
// extension.  `import.meta.dirname` needs Node.js 20.11 or newer, which the
// bundled runtime provides.
//
// Usage:
//     node scripts/build_exthost.js [--watch] [--minify]

'use strict';

const path = require('path');
const fs = require('fs');

const REPO_ROOT = path.resolve(__dirname, '..');
const ENTRY = path.join(REPO_ROOT, 'src', 'ext', 'exthost', 'extensionHostProcess.ts');
const OUTFILE = path.join(REPO_ROOT, 'src', 'ext', 'exthost', 'extensionHostProcess.mjs');

/**
 * Native or optional modules that must not be bundled. They are either
 * platform specific binaries or optional integrations that the extension host
 * degrades gracefully without.
 */
const EXTERNAL = [
    '@vscode/native-watchdog',
    '@vscode/spdlog',
    '@vscode/windows-ca-certs',
    'electron',
    'keytar',
    'kerberos',
    'native-keymap',
    'native-watchdog',
    'node-pty',
    'vscode-sqlite3',
];

async function main() {
    const args = process.argv.slice(2);
    const watch = args.includes('--watch');
    const minify = args.includes('--minify');

    let esbuild;
    try {
        esbuild = require('esbuild');
    } catch (err) {
        console.error('[build_exthost] esbuild is not installed. Run `npm install` first.');
        process.exit(1);
    }

    if (!fs.existsSync(ENTRY)) {
        console.error(`[build_exthost] missing entry point: ${ENTRY}`);
        process.exit(1);
    }

    const options = {
        entryPoints: [ENTRY],
        bundle: true,
        platform: 'node',
        format: 'esm',
        target: 'node20',
        outfile: OUTFILE,
        external: EXTERNAL,
        // The native modules are copied next to the bundle so that the
        // release does not need a node_modules directory at runtime.
        loader: {
            '.node': 'copy',
        },
        minify,
        sourcemap: minify ? false : 'linked',
        logLevel: 'info',
        legalComments: 'inline',
        banner: {
            js: [
                '/*',
                ' * Vrutti IDE -- Extension Host bundle (generated file, do not edit).',
                ' *',
                ' * Bundles the MIT licensed extension host service layer of the VS Code',
                ' * source tree. See THIRD_PARTY_NOTICES.md for the required notice.',
                ' */',
                // The service layer contains CommonJS modules that call
                // `require()` for the Node.js built-ins. An ES module has no
                // `require`, so one is created here. Without it the bundle
                // stops with "Dynamic require of ... is not supported".
                "import { createRequire as __vruttiCreateRequire } from 'module';",
                'const require = __vruttiCreateRequire(import.meta.url);',
            ].join('\n'),
        },
    };

    if (watch) {
        const context = await esbuild.context(options);
        await context.watch();
        console.log(`[build_exthost] watching -> ${OUTFILE}`);
        return;
    }

    const result = await esbuild.build(options);
    if (result.errors && result.errors.length > 0) {
        console.error(`[build_exthost] build failed with ${result.errors.length} error(s)`);
        process.exit(1);
    }

    const size = fs.statSync(OUTFILE).size;
    console.log(`[build_exthost] wrote ${OUTFILE} (${(size / 1024 / 1024).toFixed(2)} MiB)`);
}

main().catch((err) => {
    console.error('[build_exthost]', err);
    process.exit(1);
});
