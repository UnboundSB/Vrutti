// dom-bridge.ts

/**
 * Installs a DOM proxy that intercepts `querySelector` and `querySelectorAll`.
 * This allows VS Code extensions to think they are querying standard VS Code UI components,
 * preventing crashes and allowing them to inject HTML/CSS into a safe Light DOM layer.
 */
export function installDomBridge() {
    console.log('[DOM Bridge] Installing Light DOM proxies for extension compatibility...');
    
    const originalQuerySelector = document.querySelector;
    const originalQuerySelectorAll = document.querySelectorAll;

    const getExtensionLayer = () => {
        let layer = originalQuerySelector.call(document, '#vrutti-extension-layer') as HTMLElement;
        if (!layer) {
            layer = document.createElement('div') as HTMLElement;
            layer.id = 'vrutti-extension-layer';
            layer.style.position = 'absolute';
            layer.style.top = '0';
            layer.style.left = '0';
            layer.style.width = '100%';
            layer.style.height = '100%';
            layer.style.zIndex = '-1';
            layer.style.pointerEvents = 'none';
            document.body.appendChild(layer);
        }
        return layer;
    };

    document.querySelector = function(selectors: string): Element | null {
        // If they specifically want the root workbench
        if (selectors.includes('.monaco-workbench')) {
            const workbench = originalQuerySelector.call(this, '.monaco-workbench');
            if (workbench) return workbench;
        }

        // If they are looking for generic VS Code parts (sidebar, editor, statusbar)
        if (typeof selectors === 'string' && (selectors.includes('.part.') || selectors.includes('.part '))) {
            console.log(`[DOM Bridge] Intercepted query for VS Code part: ${selectors}`);
            const layer = getExtensionLayer();
            const partId = 'mock-part-' + selectors.replace(/[^a-zA-Z0-9-]/g, '-');
            
            let mockEl = originalQuerySelector.call(layer, '#' + partId);
            if (!mockEl) {
                mockEl = document.createElement('div');
                mockEl.id = partId;
                // Add the requested classes so matches() or classList works
                const classes = selectors.split('.').map(s => s.trim().split(' ')[0]).filter(Boolean);
                mockEl.className = classes.join(' ');
                
                // Allow the mock to absorb injected styles safely
                layer.appendChild(mockEl);
            }
            return mockEl;
        }

        return originalQuerySelector.call(this, selectors);
    };

    document.querySelectorAll = function(selectors: string): NodeListOf<Element> {
        if (typeof selectors === 'string' && (selectors.includes('.monaco-workbench') || selectors.includes('.part.') || selectors.includes('.part '))) {
            const single = document.querySelector.call(this, selectors);
            
            // Return a live NodeList by querying our mock container
            if (single && single.parentElement) {
                // If it's a mock node, we can query its parent
                return originalQuerySelectorAll.call(single.parentElement, '#' + single.id);
            }
            // If it's the workbench, just query body
            return originalQuerySelectorAll.call(document.body, '.monaco-workbench');
        }
        return originalQuerySelectorAll.call(this, selectors);
    };

    console.log('[DOM Bridge] Proxy installed successfully.');
}
