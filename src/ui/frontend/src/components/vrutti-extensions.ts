import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { repeat } from 'lit/directives/repeat.js';

interface ExtensionResult {
    namespace: string;
    name: string;
    displayName: string;
    description: string;
    version: string;
    iconUrl: string;
    downloadUrl: string;
    publisherDisplayName: string;
    isTheme?: boolean;
    id?: string;
    // What the extension offers: the themes of the colours, the themes of
    // the icons, the views and the rest.
    contributes?: {
        themes?: { id?: string, label?: string }[];
        iconThemes?: { id?: string, label?: string }[];
        [key: string]: any;
    };
}

@customElement('vrutti-extensions')
export class VruttiExtensions extends LitElement {
    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            color: var(--vrutti-text, #cccccc);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: var(--vrutti-surface, #252526);
            overflow: hidden;
        }
        
        .header {
            padding: 10px 20px;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--vrutti-text-bright, #ffffff);
        }

        .search-container {
            padding: 10px 20px;
            border-bottom: 1px solid var(--vrutti-surface-border, #3c3c3c);
            display: flex;
            align-items: center;
        }
        
        .search-box {
            flex: 1;
            padding: 6px 8px;
            background: var(--vrutti-surface-border, #3c3c3c);
            border: 1px solid transparent;
            color: var(--vrutti-text, #cccccc);
            border-radius: 2px;
            outline: none;
            box-sizing: border-box;
        }
        
        .search-box:focus {
            border-color: var(--vrutti-accent, #007fd4);
        }

        .clear-search-btn {
            background: transparent;
            border: none;
            color: var(--vrutti-text, #cccccc);
            cursor: pointer;
            padding: 4px;
            margin-left: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 4px;
        }

        .clear-search-btn:hover {
            background: rgba(255, 255, 255, 0.1);
            color: var(--vrutti-text-bright, #ffffff);
        }

        .results {
            flex: 1;
            overflow-y: auto;
            padding: 10px 20px;
        }

        .extension-card {
            display: flex;
            align-items: flex-start;
            padding: 10px 0;
            border-bottom: 1px solid var(--vrutti-surface-border, #3c3c3c);
            cursor: pointer;
        }

        .extension-card:hover {
            background-color: rgba(255, 255, 255, 0.05);
        }

        .ext-icon {
            width: 42px;
            height: 42px;
            margin-right: 12px;
            background: var(--vrutti-surface-border, #333333);
            border-radius: 4px;
        }

        .ext-info {
            flex: 1;
            display: flex;
            flex-direction: column;
        }

        .ext-name {
            font-size: 13px;
            font-weight: 600;
            color: var(--vrutti-text-bright, #ffffff);
            margin-bottom: 2px;
        }

        .ext-publisher {
            font-size: 11px;
            color: var(--vrutti-text, #cccccc);
            margin-bottom: 6px;
        }

        .ext-desc {
            font-size: 12px;
            color: var(--vrutti-text, #969696);
            margin-bottom: 8px;
            display: -webkit-box;
            -webkit-line-clamp: 2;
            -webkit-box-orient: vertical;
            overflow: hidden;
        }

        .install-btn {
            background: var(--vrutti-accent, #0e639c);
            color: #ffffff;
            border: none;
            padding: 4px 12px;
            border-radius: 2px;
            cursor: pointer;
            font-size: 11px;
            font-weight: 600;
            align-self: flex-start;
        }

        .install-btn:hover {
            filter: brightness(1.2);
        }
        
        .loading {
            padding: 20px;
            text-align: center;
            color: #888;
            font-style: italic;
        }

        details {
            border-bottom: 1px solid var(--vrutti-surface-border, #333333);
        }
        details > summary {
            list-style: none;
            padding: 6px 12px;
            font-size: 11px;
            font-weight: 700;
            text-transform: uppercase;
            color: var(--vrutti-text-bright, #ffffff);
            cursor: pointer;
            display: flex;
            align-items: center;
        }
        details > summary::-webkit-details-marker {
            display: none;
        }
        details > summary::before {
            content: "▸";
            display: inline-block;
            margin-right: 6px;
            font-size: 12px;
            transition: transform 0.1s;
        }
        details[open] > summary::before {
            transform: rotate(90deg);
        }
    `;

    @state() private query = '';
    @state() private results: ExtensionResult[] = [];
    @state() private installed: ExtensionResult[] = [];
    @state() private isLoading = false;
    @state() private progressMap: Map<string, number> = new Map();
    @state() private errorMessage = '';
    @state() private installError = false;
    private searchTimeout: ReturnType<typeof setTimeout> | null = null;
    private currentSearchId = 0;

    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
        if ((window as any).vruttiRequestInstalledExtensions) {
            (window as any).vruttiRequestInstalledExtensions().catch(console.error);
        }
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
        if (this.searchTimeout) {
            clearTimeout(this.searchTimeout);
            this.searchTimeout = null;
        }
    }

    private handleIpc = (e: CustomEvent) => {
        const msg = e.detail;
        if (msg.method === 'extensions/installed') {
            this.installed = (msg.params || []).map((ext: any) => ({
                ...ext,
                namespace: ext.namespace || ext.publisher || 'unknown',
                iconUrl: ext.iconUrl || (ext.icon ? `file:///${(ext.localPath || '').replace(/\\/g, '/')}/${ext.icon}` : undefined)
            }));
        } else if (msg.method === 'extensions/error') {
            // The installation did not work. The reason is shown, so that the
            // user knows that nothing happened.
            const message = (msg.params && msg.params.message) || 'the extension could not be installed';
            this.errorMessage = message;
            this.installError = true;
            console.error('[extensions]', message);
        } else if (msg.method === 'extensions/progress') {
            const { name, percentage } = msg.params;
            const newMap = new Map(this.progressMap);
            newMap.set(name, percentage);
            this.progressMap = newMap;
            
            if (percentage === 100) {
                // If it was just installed, maybe we should ask backend to resend installed list,
                // but for now let's just clear the progress after 2 seconds
                setTimeout(() => {
                    const cleanMap = new Map(this.progressMap);
                    cleanMap.delete(name);
                    this.progressMap = cleanMap;
                }, 2000);
            }
        }
    };

    private onInput(e: Event) {
        const input = e.target as HTMLInputElement;
        this.query = input.value;
        
        if (this.searchTimeout) {
            clearTimeout(this.searchTimeout);
        }
        
        this.searchTimeout = setTimeout(() => {
            this.search();
        }, 500);
    }

    private clearSearch() {
        this.query = '';
        this.results = [];
        if (this.searchTimeout) {
            clearTimeout(this.searchTimeout);
        }
    }

    private async search() {
        if (!this.query.trim()) {
            this.results = [];
            return;
        }

        this.isLoading = true;
        const searchId = ++this.currentSearchId;
        
        try {
            let json: any = { extensions: [] };
            if ((window as any).sendIpcMessage) {
                // Send IPC request. Since IPC is async and doesn't return directly via sendIpcMessage,
                // we'll need to use a dedicated binding or promise wrapper.
                // In this architecture, let's assume we mapped `vruttiSearchExtensions` as a direct sync/async binding
                // or we use a promise. If not, we can use the `invoke` pattern.
                if ((window as any).vruttiSearchExtensions) {
                    const result = await (window as any).vruttiSearchExtensions(JSON.stringify({ query: this.query }));
                    json = typeof result === 'string' ? JSON.parse(result) : result;
                }
            } else {
                // Fallback for isolated web view testing
                const res = await fetch(`https://open-vsx.org/api/-/search?query=${encodeURIComponent(this.query)}`);
                json = await res.json();
            }
            
            if (this.currentSearchId !== searchId) return;
            
            if (json.extensions) {
                this.results = json.extensions.map((ext: any) => ({
                    namespace: ext.namespace,
                    name: ext.name,
                    displayName: ext.displayName || ext.name,
                    description: ext.description,
                    version: ext.version,
                    iconUrl: ext.files?.icon,
                    downloadUrl: ext.files?.download,
                    publisherDisplayName: ext.publisherDisplayName || ext.namespace
                }));
            } else {
                this.results = [];
            }
        } catch (e) {
            if (this.currentSearchId !== searchId) return;
            console.error("OpenVSX Search failed", e);
            this.results = [];
        } finally {
            if (this.currentSearchId === searchId) {
                this.isLoading = false;
            }
        }
    }

    private install(ext: ExtensionResult, e: Event) {
        e.stopPropagation();
        console.log(`Requesting installation for ${ext.namespace}.${ext.name} from ${ext.downloadUrl}`);
        if ((window as any).vruttiInstallExtension) {
            this.progressMap.set(ext.name, 0);
            this.requestUpdate();
            try {
                const res = (window as any).vruttiInstallExtension(ext.downloadUrl, ext.name);
                if (res && typeof res.catch === 'function') {
                    res.catch(console.error);
                }
            } catch (err) {
                console.error("Install error:", err);
            }
        }
    }

    private uninstall(ext: ExtensionResult, e: Event) {
        e.stopPropagation();
        // The extension is removed by its whole name: the name of the
        // extension alone is not enough, two publishers may use the same
        // name.
        const identifier = ext.id || `${ext.namespace}.${ext.name}`;
        console.log(`Requesting uninstall for ${identifier}`);
        if ((window as any).vruttiUninstallExtension) {
            try {
                const res = (window as any).vruttiUninstallExtension(identifier);
                if (res && typeof res.catch === 'function') {
                    res.catch(console.error);
                }
            } catch (err) {
                console.error("Uninstall error:", err);
            }
        }
    }

    // The extension of the list that is installed, if it is.
    //
    // The list of the marketplace and the list of what is installed are two
    // lists. The themes of an extension are only in the second one, so the
    // themes of the button are taken from there.
    private installedEntry(ext: ExtensionResult): ExtensionResult | undefined {
        const extId = `${ext.namespace}.${ext.name}`.toLowerCase();
        const extName = (ext.name || '').toLowerCase();
        return this.installed.find(i => {
            const iId = (i.id || '').toLowerCase();
            const iName = (i.name || '').toLowerCase();
            return iId === extId || iName === extName;
        });
    }

    // The buttons that apply the themes of an extension.
    //
    // A theme of the colours and a theme of the icons are two different
    // things: the core keeps them apart, and a button that sends a theme of
    // the colours for a theme of the icons does nothing at all.
    private themeButtons(ext: ExtensionResult) {
        const entry = this.installedEntry(ext);
        if (!entry) return html``;
        const themes = entry.contributes?.themes;
        const iconThemes = entry.contributes?.iconThemes;
        const hasColour = Array.isArray(themes) && themes.length > 0;
        const hasIcon = Array.isArray(iconThemes) && iconThemes.length > 0;
        if (!hasColour && !hasIcon) return html``;

        return html`
            ${hasColour ? html`<button class="install-btn" @click=${(e: Event) => this.setTheme(ext, e, 'colour')}>Set Theme</button>` : ''}
            ${hasIcon ? html`<button class="install-btn" @click=${(e: Event) => this.setTheme(ext, e, 'icon')}>Set Icon Theme</button>` : ''}
        `;
    }

    private setTheme(ext: ExtensionResult, e: Event, kind: 'colour' | 'icon') {
        e.stopPropagation();

        const entry = this.installedEntry(ext) || ext;
        const list = kind === 'colour'
            ? entry.contributes?.themes
            : entry.contributes?.iconThemes;
        const theme = Array.isArray(list) && list.length > 0 ? list[0] : undefined;

        // The name of a theme in the core is the name of the extension that
        // offers it and the name of the theme, with a dot between them.
        const name = theme && theme.id ? `${entry.name}.${theme.id}` : entry.name;
        console.log(`Setting the ${kind} theme '${name}'`);

        if ((window as any).sendIpcMessage) {
            (window as any).sendIpcMessage(
                kind === 'colour' ? 'theme/set' : 'icon_theme/set',
                JSON.stringify({ name }));
        }
    }

    private selectExtension(ext: ExtensionResult, isInstalled: boolean) {
        const detailObj = { ...ext, isInstalled };
        this.dispatchEvent(new CustomEvent('extension-selected', {
            detail: detailObj,
            bubbles: true,
            composed: true
        }));
    }

    renderExtensionList(list: ExtensionResult[]) {
        return html`
            ${repeat(list, ext => `${ext.namespace}.${ext.name}`, ext => {
                const progress = this.progressMap.get(ext.name);
                const isInstalling = progress !== undefined;
                const extId = `${ext.namespace}.${ext.name}`.toLowerCase();
                const extName = (ext.name || '').toLowerCase();
                const isInstalled = this.installed.some(i => {
                    const iId = (i.id || '').toLowerCase();
                    const iName = (i.name || '').toLowerCase();
                    return iId === extId || iName === extName;
                });
                return html`
                <div class="extension-card" @click=${() => this.selectExtension(ext, isInstalled)}>
                    <img class="ext-icon" src=${ext.iconUrl || 'data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" fill="%23333"/><text x="50" y="50" fill="%23888" font-size="40" text-anchor="middle" dominant-baseline="middle">E</text></svg>'} @error=${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                    <div class="ext-info">
                        <div class="ext-name">${ext.displayName}</div>
                        <div class="ext-publisher">${ext.publisherDisplayName}</div>
                        <div class="ext-desc" title=${ext.description}>${ext.description}</div>
                        
                        ${isInstalling ? html`
                            <div style="width: 100%; height: 4px; background: #333; margin-top: 4px; border-radius: 2px; overflow: hidden;">
                                <div style="width: ${progress}%; height: 100%; background: var(--vrutti-accent, #007fd4); transition: width 0.2s;"></div>
                            </div>
                        ` : (isInstalled ? html`
                            <div style="display: flex; gap: 8px;">
                                <span style="font-size: 11px; color: var(--vrutti-text, #888); background: var(--vrutti-surface-border, #333); padding: 2px 6px; border-radius: 4px; align-self: flex-start;">Installed</span>
                                <button class="install-btn" style="background: #e81123;" @click=${(e: Event) => this.uninstall(ext, e)}>Uninstall</button>
                                ${this.themeButtons(ext)}
                            </div>
                        ` : html`
                            <button class="install-btn" @click=${(e: Event) => this.install(ext, e)}>Install</button>
                        `)}
                    </div>
                </div>
            `})}
        `;
    }

    render() {
        return html`
            <div class="header">EXTENSIONS</div>
            <div class="search-container">
                <input type="text" class="search-box" placeholder="Search Extensions in Marketplace" .value=${this.query} @input=${this.onInput} />
                ${this.query ? html`
                    <button class="clear-search-btn" @click=${this.clearSearch} title="Clear Search">
                        <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M4.646 4.646a.5.5 0 0 1 .708 0L8 7.293l2.646-2.647a.5.5 0 0 1 .708.708L8.707 8l2.647 2.646a.5.5 0 0 1-.708.708L8 8.707l-2.646 2.647a.5.5 0 0 1-.708-.708L7.293 8 4.646 5.354a.5.5 0 0 1 0-.708z"/></svg>
                    </button>
                ` : ''}
            </div>
            <div class="results">
                ${this.installError ? html`
                    <div class="loading" style="color: #f48771;">
                        ${this.errorMessage}
                    </div>
                ` : ''}
                ${this.query ? html`
                    <details open>
                        <summary>MARKETPLACE (${this.results.length})</summary>
                        ${this.isLoading ? html`<div class="loading">Searching Open VSX Registry...</div>` : ''}
                        ${!this.isLoading && this.results.length === 0 ? html`<div class="loading">No extensions found.</div>` : ''}
                        ${this.renderExtensionList(this.results)}
                    </details>
                ` : html`
                    <details open>
                        <summary>INSTALLED (${this.installed.length})</summary>
                        ${this.installed.length === 0 ? html`<div class="loading">No extensions installed.</div>` : ''}
                        ${this.renderExtensionList(this.installed)}
                    </details>
                `}
            </div>
        `;
    }
}
