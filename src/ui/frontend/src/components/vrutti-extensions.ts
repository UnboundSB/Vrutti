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
        }
        
        .search-box {
            width: 100%;
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
    @state() private isLoading = false;
    private searchTimeout: any;

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

    private async search() {
        if (!this.query.trim()) {
            this.results = [];
            return;
        }

        this.isLoading = true;
        try {
            const res = await fetch(`https://open-vsx.org/api/-/search?query=${encodeURIComponent(this.query)}`);
            const json = await res.json();
            if (json.extensions) {
                this.results = json.extensions.map((ext: any) => ({
                    namespace: ext.namespace,
                    name: ext.name,
                    displayName: ext.displayName || ext.name,
                    description: ext.description,
                    version: ext.version,
                    iconUrl: ext.files.icon,
                    downloadUrl: ext.files.download,
                    publisherDisplayName: ext.publisherDisplayName || ext.namespace
                }));
            } else {
                this.results = [];
            }
        } catch (e) {
            console.error("OpenVSX Search failed", e);
            this.results = [];
        } finally {
            this.isLoading = false;
        }
    }

    private install(ext: ExtensionResult) {
        console.log(`Requesting installation for ${ext.namespace}.${ext.name} from ${ext.downloadUrl}`);
        if ((window as any).vruttiInstallExtension) {
            (window as any).vruttiInstallExtension(ext.downloadUrl, ext.name).catch(console.error);
        }
    }

    render() {
        return html`
            <div class="header">EXTENSIONS</div>
            <div class="search-container">
                <input type="text" class="search-box" placeholder="Search Extensions in Marketplace" .value=${this.query} @input=${this.onInput} />
            </div>
            <div class="results">
                ${this.isLoading ? html`<div class="loading">Searching Open VSX Registry...</div>` : ''}
                ${!this.isLoading && this.results.length === 0 && this.query ? html`<div class="loading">No extensions found.</div>` : ''}
                ${this.results.map(ext => html`
                    <div class="extension-card">
                        <img class="ext-icon" src=${ext.iconUrl} @error=${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                        <div class="ext-info">
                            <div class="ext-name">${ext.displayName}</div>
                            <div class="ext-publisher">${ext.publisherDisplayName}</div>
                            <div class="ext-desc" title=${ext.description}>${ext.description}</div>
                            <button class="install-btn" @click=${() => this.install(ext)}>Install</button>
                        </div>
                    </div>
                `)}
            </div>
        `;
    }
}
