// Builds the Vrutti Extension Host bundle.
//
// The extension host is the MIT licensed service layer of the VS Code source
// tree (see THIRD_PARTY_NOTICES.md).  At development time it can be run
// straight from TypeScript with `tsx`, but a release must not depend on a
// TypeScript loader -- it has to run on the bundled Node.js runtime with a
// plain `.js` file.
//
// This script bundles `src/ext/exthost/extensionHostProcess.ts` and everything
// it pulls in into a single CommonJS file:
//
//     src/ext/exthost/extensionHostProcess.js
//
// Usage:
//     node scripts/build_exthost.js [--watch] [--minify]

'use strict';

const path = require('path');
const fs = require('fs');

const REPO_ROOT = path.resolve(__dirname, '..');
const ENTRY = path.join(REPO_ROOT, 'src', 'ext', 'exthost', 'extensionHostProcess.ts');
const OUTFILE = path.join(REPO_ROOT, 'src', 'ext', 'exthost', 'extensionHostProcess.js');

/**
 * Native or optional modules that must not be bundled. They are either
 * platform specific binaries or optional integrations that the extension host
 * degrades gracefully without.
 */
const EXTERNAL = [
    '@vscode/native-watchdog',
    '@vscode/spdlog',
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
        format: 'cjs',
        target: 'node18',
        outfile: OUTFILE,
        external: EXTERNAL,
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
