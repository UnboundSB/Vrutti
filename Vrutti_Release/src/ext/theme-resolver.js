const fs = require('fs');
const path = require('path');

/**
 * Resolves a VS Code theme JSON recursively, merging included themes.
 * @param {string} themePath Path to the root theme JSON file.
 * @returns {Promise<Object>} Fully resolved theme object.
 */
async function loadThemeRecursive(themePath) {
    const loadedPaths = new Set();
    
    async function load(currentPath) {
        if (loadedPaths.has(currentPath)) return {}; // Prevent infinite loops
        loadedPaths.add(currentPath);
        
        let raw;
        try {
            raw = await fs.promises.readFile(currentPath, 'utf8');
        } catch (e) {
            console.error(`Failed to read theme file: ${currentPath}`, e);
            return {};
        }
        
        // Strip comments (JSON with comments is standard for VS Code themes)
        raw = raw.replace(/\/\*([\s\S]*?)\*\/|([^\\:]|^)\/\/.*$/gm, '$2');
        
        let themeObj;
        try {
            themeObj = JSON.parse(raw);
        } catch (e) {
            console.error(`Failed to parse theme JSON: ${currentPath}`, e);
            return {};
        }
        
        const result = { ...themeObj, colors: {}, tokenColors: [] };
        
        if (themeObj.include) {
            const includePath = path.resolve(path.dirname(currentPath), themeObj.include);
            const parentTheme = await load(includePath);
            
            // Merge parent properties first, then overwrite with current theme properties
            Object.assign(result, parentTheme, result);

            Object.assign(result.colors, parentTheme.colors || {});
            if (parentTheme.tokenColors) {
                result.tokenColors = result.tokenColors.concat(parentTheme.tokenColors);
            }
        }
        
        if (themeObj.colors) {
            Object.assign(result.colors, themeObj.colors);
        }
        
        if (themeObj.tokenColors) {
            // Note: some themes format tokenColors slightly differently but standard is array
            if (Array.isArray(themeObj.tokenColors)) {
                result.tokenColors = result.tokenColors.concat(themeObj.tokenColors);
            } else if (typeof themeObj.tokenColors === 'string') {
                // if it's a file path reference (rare but possible in textmate)
                // usually not the case in modern vscode themes
            }
        }
        
        // Keep top level props from the outermost file mostly
        result.name = themeObj.name || result.name;
        result.type = themeObj.type || result.type;
        
        return result;
    }
    
    return load(themePath);
}

module.exports = {
    loadThemeRecursive
};
