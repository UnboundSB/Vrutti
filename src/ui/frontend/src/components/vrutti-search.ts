import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-search')
export class VruttiSearch extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            font-family: var(--vrutti-font, 'Inter', -apple-system, sans-serif);
            font-size: 13px;
            color: var(--vrutti-text);
            background: var(--vrutti-bg-dark, #1e1e1e);
            overflow: hidden;
        }

        .search-container {
            padding: 12px;
            display: flex;
            flex-direction: column;
            gap: 8px;
            border-bottom: 1px solid var(--vrutti-surface-border, #2a2e42);
        }

        .input-group {
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        label {
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            color: #a9b1d6;
        }

        input {
            width: 100%;
            box-sizing: border-box;
            background: #1a1b26;
            border: 1px solid #3b4261;
            color: #c0caf5;
            padding: 6px 8px;
            border-radius: 4px;
            font-family: inherit;
            font-size: 13px;
            outline: none;
            transition: border-color 0.2s;
        }

        input:focus {
            border-color: #7aa2f7;
        }

        .results-container {
            flex: 1;
            overflow-y: auto;
            padding: 8px 0;
        }

        .result-item {
            padding: 4px 12px;
            cursor: pointer;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            transition: background 0.1s;
        }

        .result-item:hover {
            background: #292e42;
            color: #7aa2f7;
        }

        .no-results {
            padding: 12px;
            color: #565f89;
            text-align: center;
        }
    `];

    @state() private query = '';
    @state() private directory = '.';
    @state() private results: string[] = [];
    @state() private isSearching = false;

    private async performSearch() {
        if (!this.query.trim()) {
            this.results = [];
            return;
        }

        this.isSearching = true;
        try {
            const req = {
                query: this.query,
                directory: this.directory
            };
            
            if ((window as any).vruttiSearch) {
                const resStr = await (window as any).vruttiSearch([req]);
                this.results = JSON.parse(resStr);
            }
        } catch (e) {
            console.error("Search failed:", e);
            this.results = [];
        } finally {
            this.isSearching = false;
        }
    }

    private handleQueryChange(e: Event) {
        this.query = (e.target as HTMLInputElement).value;
    }

    private handleQueryKeydown(e: KeyboardEvent) {
        if (e.key === 'Enter') {
            this.performSearch();
        }
    }

    private handleDirChange(e: Event) {
        this.directory = (e.target as HTMLInputElement).value;
    }
    
    private handleResultClick(result: string) {
        console.log("Clicked search result:", result);
        const pathMatch = result.match(/at\s+(.+?)(?::\d+)?$/);
        if (pathMatch && pathMatch[1]) {
            let path = pathMatch[1];
            if (!path.startsWith("file:///")) {
                path = "file:///" + path;
            }
            
            this.dispatchEvent(new CustomEvent('open-file', {
                detail: { path: path, name: path.split(/[\\/]/).pop() },
                bubbles: true,
                composed: true
            }));
        }
    }

    render() {
        return html`
            <div class="search-container">
                <div class="input-group">
                    <label>Search</label>
                    <input 
                        type="text" 
                        placeholder="Search query..." 
                        .value=${this.query}
                        @input=${this.handleQueryChange}
                        @keydown=${this.handleQueryKeydown}
                    />
                </div>
                <div class="input-group">
                    <label>Files to include</label>
                    <input 
                        type="text" 
                        placeholder="e.g. src/ or .cpp" 
                        .value=${this.directory}
                        @input=${this.handleDirChange}
                        @keydown=${this.handleQueryKeydown}
                    />
                </div>
            </div>
            
            <div class="results-container">
                ${this.isSearching ? html`<div class="no-results">Searching...</div>` : ''}
                ${!this.isSearching && this.results.length === 0 && this.query ? html`<div class="no-results">No results found.</div>` : ''}
                
                ${this.results.map(res => html`
                    <div class="result-item" @click=${() => this.handleResultClick(res)} title=${res}>
                        ${res}
                    </div>
                `)}
            </div>
        `;
    }
}
