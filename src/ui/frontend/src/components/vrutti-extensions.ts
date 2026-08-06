import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface ExtensionResult {
    namespace: string;
    name: string;
    displayName: string;
    description: string;
    version: string;
    iconUrl: string;
    downloadUrl: string;
    publisherDisplayName: string;
}

@customElement('vrutti-extensions')
export class VruttiExtensions extends LitElement {
    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            color: #cccccc;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background: #252526;
            overflow: hidden;
        }
        
        .header {
            padding: 10px 20px;
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #ffffff;
        }

        .search-container {
            padding: 10px 20px;
            border-bottom: 1px solid #3c3c3c;
            display: flex;
            align-items: center;
        }
        
        .search-box {
            flex: 1;
            padding: 6px 8px;
            background: #3c3c3c;
            border: 1px solid transparent;
            color: #cccccc;
            border-radius: 2px;
            outline: none;
            box-sizing: border-box;
        }
        
        .search-box:focus {
            border-color: #007fd4;
        }

        .clear-search-btn {
            background: transparent;
            border: none;
            color: #cccccc;
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
            color: #ffffff;
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
            border-bottom: 1px solid #3c3c3c;
            cursor: pointer;
        }

        .extension-card:hover {
            background-color: #2a2d2e;
        }

        .ext-icon {
            width: 42px;
            height: 42px;
            margin-right: 12px;
            background: #333333;
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
            color: #ffffff;
            margin-bottom: 2px;
        }

        .ext-publisher {
            font-size: 11px;
            color: #cccccc;
            margin-bottom: 6px;
        }

        .ext-desc {
            font-size: 12px;
            color: #969696;
            margin-bottom: 8px;
            display: -webkit-box;
            -webkit-line-clamp: 2;
            -webkit-box-orient: vertical;
            overflow: hidden;
        }

        .install-btn {
            background: #0e639c;
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
            background: #1177bb;
        }
        
        .loading {
            padding: 20px;
            text-align: center;
            color: #888;
            font-style: italic;
        }
    `;

    @state() private query = '';
    @state() private results: ExtensionResult[] = [];
    @state() private installed: ExtensionResult[] = [];
    @state() private isLoading = false;
    @state() private progressMap: Map<string, number> = new Map();
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
    }

    private handleIpc = (e: CustomEvent) => {
        const msg = e.detail;
        if (msg.method === 'extensions/installed') {
            this.installed = msg.params || [];
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
            const res = await fetch(`https://open-vsx.org/api/-/search?query=${encodeURIComponent(this.query)}`);
            const json = await res.json();
            
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
            (window as any).vruttiInstallExtension(ext.downloadUrl, ext.name).catch(console.error);
        }
    }

    private uninstall(ext: ExtensionResult, e: Event) {
        e.stopPropagation();
        console.log(`Requesting uninstall for ${ext.name}`);
        if ((window as any).vruttiUninstallExtension) {
            (window as any).vruttiUninstallExtension(ext.name).catch(console.error);
        }
    }

    private selectExtension(ext: ExtensionResult) {
        this.dispatchEvent(new CustomEvent('extension-selected', {
            detail: ext,
            bubbles: true,
            composed: true
        }));
    }

    render() {
        const displayList = this.query ? this.results : this.installed;
        
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
                ${this.isLoading ? html`<div class="loading">Searching Open VSX Registry...</div>` : ''}
                ${!this.isLoading && this.query && this.results.length === 0 ? html`<div class="loading">No extensions found.</div>` : ''}
                ${!this.isLoading && !this.query && this.installed.length === 0 ? html`<div class="loading">No extensions installed.</div>` : ''}
                
                ${displayList.map(ext => {
                    const progress = this.progressMap.get(ext.name) || 0;
                    const isInstalling = progress > 0 && progress < 100;
                    return html`
                    <div class="extension-card" @click=${() => this.selectExtension(ext)}>
                        <img class="ext-icon" src=${ext.iconUrl || 'data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" fill="%23333"/><text x="50" y="50" fill="%23888" font-size="40" text-anchor="middle" dominant-baseline="middle">E</text></svg>'} @error=${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                        <div class="ext-info">
                            <div class="ext-name">${ext.displayName}</div>
                            <div class="ext-publisher">${ext.publisherDisplayName}</div>
                            <div class="ext-desc" title=${ext.description}>${ext.description}</div>
                            
                            ${isInstalling ? html`
                                <div style="width: 100%; height: 4px; background: #333; margin-top: 4px; border-radius: 2px; overflow: hidden;">
                                    <div style="width: ${progress}%; height: 100%; background: #007fd4; transition: width 0.2s;"></div>
                                </div>
                            ` : (!this.query ? html`
                                <div style="display: flex; gap: 8px;">
                                    <span style="font-size: 11px; color: #888; background: #333; padding: 2px 6px; border-radius: 4px; align-self: flex-start;">Installed</span>
                                    <button class="install-btn" style="background: #e81123;" @click=${(e: Event) => this.uninstall(ext, e)}>Uninstall</button>
                                </div>
                            ` : html`
                                <button class="install-btn" @click=${(e: Event) => this.install(ext, e)}>Install</button>
                            `)}
                        </div>
                    </div>
                `})}
            </div>
        `;
    }
}
