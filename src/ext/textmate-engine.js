const fs = require('fs');
const path = require('path');
const vsctm = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

let registry = null;
let grammarCache = {};
let scopeToLanguageMap = {};

async function initOniguruma() {
    if (registry) return;
    
    const wasmPath = path.join(require.resolve('vscode-oniguruma'), '../../onig.wasm');
    const wasmBin = await fs.promises.readFile(wasmPath);
    
    const onigLib = oniguruma.loadWASM(wasmBin.buffer).then(() => {
        return {
            createOnigScanner(patterns) { return new oniguruma.OnigScanner(patterns); },
            createOnigString(s) { return new oniguruma.OnigString(s); }
        };
    });
    
    registry = new vsctm.Registry({
        onigLib,
        loadGrammar: async (scopeName) => {
            const grammarPath = grammarCache[scopeName];
            if (grammarPath) {
                const content = await fs.promises.readFile(grammarPath, 'utf8');
                return vsctm.parseRawGrammar(content, grammarPath);
            }
            return null;
        }
    });
}

/**
 * Registers grammars from an extension's package.json
 */
function registerGrammars(extensionPath, contributes) {
    if (contributes && contributes.grammars) {
        for (const g of contributes.grammars) {
            // In Vrutti, extensions are unzipped such that package.json is inside the 'extension' folder
            const grammarPath = path.join(extensionPath, 'extension', g.path);
            grammarCache[g.scopeName] = grammarPath;
            if (g.language) {
                scopeToLanguageMap[g.language] = g.scopeName;
            }
        }
    }
}

/**
 * Tokenizes the full document and returns line-by-line tokens
 * suitable for sending to the frontend.
 */
async function tokenizeDocument(languageId, text) {
    await initOniguruma();
    
    const scopeName = scopeToLanguageMap[languageId];
    if (!scopeName) {
        return null; // Grammar not found for language
    }
    
    const grammar = await registry.loadGrammar(scopeName);
    if (!grammar) return null;
    
    const lines = text.split('\n');
    let ruleStack = vsctm.INITIAL;
    const result = [];
    
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        const lineTokens = grammar.tokenizeLine(line, ruleStack);
        
        // We only need start index, end index, and scopes for each token
        const simplifiedTokens = lineTokens.tokens.map(t => ({
            startIndex: t.startIndex,
            endIndex: t.endIndex,
            scopes: t.scopes
        }));
        
        result.push(simplifiedTokens);
        ruleStack = lineTokens.ruleStack;
    }
    
    return result;
}

module.exports = {
    initOniguruma,
    registerGrammars,
    tokenizeDocument
};
