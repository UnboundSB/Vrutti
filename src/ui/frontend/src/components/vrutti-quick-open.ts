import { LitElement, html, css } from 'lit';
import { customElement, state, query } from 'lit/decorators.js';
import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-quick-open')
export class VruttiQuickOpen extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: block;
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            z-index: 10000;
            pointer-events: none;
        }

        .overlay {
            position: absolute;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0, 0, 0, 0.4);
            pointer-events: auto;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            padding-top: 10vh;
        }

        .palette {
            width: 600px;
            max-width: 90vw;
            background: var(--vrutti-bg-dark, #1e1e1e);
            border: 1px solid #3b4261;
            border-radius: 8px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
            display: flex;
            flex-direction: column;
            overflow: hidden;
            font-family: var(--vrutti-font, 'Inter', -apple-system, sans-serif);
        }

        .input-container {
            padding: 12px;
            border-bottom: 1px solid #3b4261;
        }

        input {
            width: 100%;
            background: #1a1b26;
            border: 1px solid #7aa2f7;
            border-radius: 4px;
            color: #c0caf5;
            padding: 8px 12px;
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-sizing: border-box;
        }

        .results {
            max-height: 400px;
            overflow-y: auto;
            padding: 4px 0;
        }

        .result-item {
            padding: 6px 16px;
            cursor: pointer;
            color: #c0caf5;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .result-item.selected {
            background: rgba(122, 162, 247, 0.2);
        }

        .result-item:hover {
            background: #292e42;
        }

        .file-name {
            font-weight: 500;
        }

        .file-path {
            color: #565f89;
            font-size: 11px;
            margin-left: auto;
        }

        .no-results {
            padding: 12px;
            color: #565f89;
            text-align: center;
        }
    `];

    @state() private query = '';
    @state() private files: string[] = [];
    @state() private filteredFiles: string[] = [];
    @state() private selectedIndex = 0;
    
    @query('input') private inputEl!: HTMLInputElement;

    connectedCallback() {
        super.connectedCallback();
        this.fetchFiles();
        window.addEventListener('keydown', this.handleGlobalKeydown);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('keydown', this.handleGlobalKeydown);
    }

    protected firstUpdated() {
        setTimeout(() => this.inputEl?.focus(), 50);
    }

    private handleGlobalKeydown = (e: KeyboardEvent) => {
        if (e.key === 'Escape') {
            this.close();
        }
    };

    private async fetchFiles() {
        try {
            let actualDir = (window as any).currentWorkspace || '.';
            if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
            else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);

            if ((window as any).vruttiSearch) {
                const req = { directory: actualDir };
                const resStr = await (window as any).vruttiSearch([{ command: "find_files", ...req }]);
                // Fallback in case search plugin doesn't route properly
                let parsed = JSON.parse(resStr);
                
                // If it returned search results instead of string array, we might need to adjust
                // But we implemented find_files to return array of strings
                this.files = parsed;
                this.filterFiles();
            }
        } catch (e) {
            console.error("Failed to fetch files:", e);
        }
    }

    private filterFiles() {
        const q = this.query.toLowerCase();
        if (!q) {
            this.filteredFiles = this.files.slice(0, 50);
        } else {
            this.filteredFiles = this.files.filter(f => {
                const lowerF = f.toLowerCase();
                // Check exact substring match (equality/similarity)
                if (lowerF.includes(q)) return true;
                
                // Fuzzy match
                let qIndex = 0;
                for (let i = 0; i < lowerF.length; i++) {
                    if (lowerF[i] === q[qIndex]) {
                        qIndex++;
                        if (qIndex === q.length) return true;
                    }
                }
                return false;
            }).slice(0, 50);
        }
        this.selectedIndex = 0;
    }

    private handleInput(e: Event) {
        this.query = (e.target as HTMLInputElement).value;
        this.filterFiles();
    }

    private handleKeydown(e: KeyboardEvent) {
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            this.selectedIndex = Math.min(this.selectedIndex + 1, this.filteredFiles.length - 1);
            this.scrollToSelected();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
            this.scrollToSelected();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (this.filteredFiles[this.selectedIndex]) {
                this.openFile(this.filteredFiles[this.selectedIndex]);
            }
        }
    }

    private scrollToSelected() {
        const resultsEl = this.shadowRoot?.querySelector('.results');
        const selectedEl = this.shadowRoot?.querySelector('.result-item.selected') as HTMLElement;
        if (resultsEl && selectedEl) {
            const containerRect = resultsEl.getBoundingClientRect();
            const itemRect = selectedEl.getBoundingClientRect();
            if (itemRect.bottom > containerRect.bottom) {
                resultsEl.scrollTop += itemRect.bottom - containerRect.bottom;
            } else if (itemRect.top < containerRect.top) {
                resultsEl.scrollTop -= containerRect.top - itemRect.top;
            }
        }
    }

    private openFile(path: string) {
        let fullPath = path;
        if (!fullPath.startsWith("file:///")) {
            fullPath = "file:///" + fullPath;
        }
        this.dispatchEvent(new CustomEvent('open-file', {
            detail: { path: fullPath, name: fullPath.split(/[\\/]/).pop(), line: 1 },
            bubbles: true,
            composed: true
        }));
        this.close();
    }

    private close() {
        this.dispatchEvent(new CustomEvent('close-quick-open', { bubbles: true, composed: true }));
    }

    render() {
        return html`
            <div class="overlay" @mousedown=${(e: MouseEvent) => { if (e.target === e.currentTarget) this.close(); }}>
                <div class="palette">
                    <div class="input-container">
                        <input 
                            type="text" 
                            placeholder="Search files by name..." 
                            .value=${this.query}
                            @input=${this.handleInput}
                            @keydown=${this.handleKeydown}
                        />
                    </div>
                    <div class="results">
                        ${this.filteredFiles.length === 0 ? html`<div class="no-results">No matching files found.</div>` : ''}
                        ${this.filteredFiles.map((file, i) => html`
                            <div class="result-item ${i === this.selectedIndex ? 'selected' : ''}" 
                                 @click=${() => this.openFile(file)}
                                 @mouseenter=${() => this.selectedIndex = i}>
                                <span class="file-name">${file.split(/[\\/]/).pop()}</span>
                                <span class="file-path">${file}</span>
                            </div>
                        `)}
                    </div>
                </div>
            </div>
        `;
    }
}
