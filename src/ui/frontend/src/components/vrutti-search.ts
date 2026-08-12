import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { globalHoverStyle } from '../shared-styles';
import { icon_chevron_down, icon_replace_all } from './codicons';

interface SearchResult {
    file: string;
    line: number;
    text: string;
}

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
            padding: 12px 8px;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .search-row {
            display: flex;
            align-items: flex-start;
            gap: 4px;
        }

        .toggle-chevron {
            width: 20px;
            height: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            color: #636b95;
            margin-top: 2px;
        }

        .toggle-chevron:hover {
            color: #a6accd;
        }

        .toggle-chevron svg {
            width: 14px;
            height: 14px;
            transition: transform 0.2s;
        }

        .toggle-chevron.expanded svg {
            transform: rotate(0deg);
        }
        .toggle-chevron:not(.expanded) svg {
            transform: rotate(-90deg);
        }

        .input-wrapper {
            position: relative;
            flex: 1;
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .input-box {
            position: relative;
            display: flex;
            align-items: center;
            background: #1a1b26;
            border: 1px solid #3b4261;
            border-radius: 4px;
            padding-right: 70px;
        }
        
        .input-box.replace {
            padding-right: 24px;
        }

        .input-box:focus-within {
            border-color: #7aa2f7;
        }

        input {
            width: 100%;
            background: transparent;
            border: none;
            color: #c0caf5;
            padding: 4px 6px;
            font-family: inherit;
            font-size: 13px;
            outline: none;
        }

        .input-actions {
            position: absolute;
            right: 2px;
            display: flex;
            align-items: center;
            gap: 2px;
        }

        .action-btn {
            width: 20px;
            height: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            color: #636b95;
            border-radius: 3px;
            font-weight: 600;
            font-size: 11px;
            user-select: none;
        }

        .action-btn svg {
            width: 14px;
            height: 14px;
        }

        .action-btn:hover {
            background: rgba(255, 255, 255, 0.1);
            color: #a6accd;
        }

        .action-btn.active {
            background: rgba(122, 162, 247, 0.2);
            color: #7aa2f7;
        }

        .results-container {
            flex: 1;
            overflow-y: auto;
            padding: 0;
            margin-top: 8px;
        }

        .file-group {
            margin-bottom: 8px;
        }

        .file-header {
            padding: 4px 8px 4px 24px;
            font-weight: 600;
            color: #a9b1d6;
            background: #1a1b26;
            font-size: 11px;
            display: flex;
            align-items: center;
            gap: 6px;
            word-break: break-all;
        }

        .result-item {
            padding: 2px 8px 2px 36px;
            cursor: pointer;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            color: #c0caf5;
            transition: background 0.1s;
            display: flex;
            gap: 8px;
        }

        .result-item:hover {
            background: #292e42;
        }

        .line-num {
            color: #565f89;
            min-width: 24px;
            text-align: right;
        }

        .no-results {
            padding: 12px;
            color: #565f89;
            text-align: center;
        }

        .highlight {
            background-color: rgba(122, 162, 247, 0.4);
            color: #ffffff;
            border-radius: 2px;
            padding: 0 2px;
        }
        
        .result-text {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            flex: 1;
        }
    `];

    @state() private query = '';
    @state() private replaceString = '';
    @state() private directory = '.';
    @state() private results: { files: string[], folders: string[], words: SearchResult[] } = { files: [], folders: [], words: [] };
    @state() private isSearching = false;
    
    @state() private replaceExpanded = false;
    @state() private matchCase = false;
    @state() private wholeWord = false;
    @state() private useRegex = false;
    
    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('vrutti-ipc', this.handleIpc);
    }
    
    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('vrutti-ipc', this.handleIpc);
    }
    
    private handleIpc = (e: Event) => {
        const msg = (e as CustomEvent).detail;
        if (msg && msg.method === 'search/results') {
            this.results = msg.params || { files: [], folders: [], words: [] };
            this.isSearching = false;
        }
    };

    private async performSearch(isReplace: boolean = false) {
        if (!this.query.trim()) {
            this.results = { files: [], folders: [], words: [] };
            return;
        }

        this.isSearching = true;
        try {
            let actualDir = this.directory;
            if (!actualDir || actualDir === '.') {
                actualDir = (window as any).currentWorkspace || '.';
                if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
                else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);
            }

            const req = {
                query: this.query,
                directory: actualDir,
                matchCase: this.matchCase,
                wholeWord: this.wholeWord,
                useRegex: this.useRegex,
                isReplace: isReplace,
                replaceString: this.replaceString
            };
            
            if ((window as any).sendIpcMessage) {
                (window as any).sendIpcMessage('search/query', JSON.stringify(req));
                // The results will be received asynchronously via vrutti-ipc event
            }
        } catch (e) {
            console.error("Search failed:", e);
            this.results = { files: [], folders: [], words: [] };
            this.isSearching = false;
        }
    }

    private handleQueryKeydown(e: KeyboardEvent) {
        if (e.key === 'Enter') {
            this.performSearch(false);
        }
    }

    private handleReplaceKeydown(e: KeyboardEvent) {
        if (e.key === 'Enter') {
            this.performSearch(true);
        }
    }

    private handleResultClick(res: SearchResult) {
        let path = res.file;
        if (!path.startsWith("file:///")) {
            path = "file:///" + path;
        }
        
        this.dispatchEvent(new CustomEvent('open-file', {
            detail: { path: path, name: path.split(/[\\/]/).pop(), line: res.line },
            bubbles: true,
            composed: true
        }));
    }

    private groupResultsByFile() {
        const groups: Record<string, SearchResult[]> = {};
        for (const res of this.results.words || []) {
            if (!groups[res.file]) groups[res.file] = [];
            groups[res.file].push(res);
        }
        return groups;
    }

    private renderHighlightedText(text: string) {
        if (!this.query) return html`${text}`;
        
        try {
            let re: RegExp;
            if (this.useRegex) {
                re = new RegExp(this.query, this.matchCase ? 'g' : 'gi');
            } else {
                const escaped = this.query.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
                if (this.wholeWord) {
                    re = new RegExp(`\\b${escaped}\\b`, this.matchCase ? 'g' : 'gi');
                } else {
                    re = new RegExp(escaped, this.matchCase ? 'g' : 'gi');
                }
            }
            
            const parts = text.split(re);
            const matches = text.match(re);
            
            if (!matches || matches.length === 0) return html`${text}`;
            
            const result = [];
            for (let i = 0; i < parts.length; i++) {
                result.push(html`${parts[i]}`);
                if (i < matches.length) {
                    result.push(html`<mark class="highlight">${matches[i]}</mark>`);
                }
            }
            return result;
        } catch (e) {
            return html`${text}`;
        }
    }

    render() {
        const grouped = this.groupResultsByFile();
        
        return html`
            <div class="search-container">
                <div class="search-row">
                    <div class="toggle-chevron ${this.replaceExpanded ? 'expanded' : ''}" @click=${() => this.replaceExpanded = !this.replaceExpanded}>
                        ${unsafeSVG(icon_chevron_down)}
                    </div>
                    
                    <div class="input-wrapper">
                        <div class="input-box">
                            <input 
                                type="text" 
                                placeholder="Search" 
                                .value=${this.query}
                                @input=${(e: Event) => this.query = (e.target as HTMLInputElement).value}
                                @keydown=${this.handleQueryKeydown}
                            />
                            <div class="input-actions">
                                <div class="action-btn ${this.matchCase ? 'active' : ''}" @click=${() => { this.matchCase = !this.matchCase; this.performSearch(); }} title="Match Case (Alt+C)">Aa</div>
                                <div class="action-btn ${this.wholeWord ? 'active' : ''}" @click=${() => { this.wholeWord = !this.wholeWord; this.performSearch(); }} title="Match Whole Word (Alt+W)">ab</div>
                                <div class="action-btn ${this.useRegex ? 'active' : ''}" @click=${() => { this.useRegex = !this.useRegex; this.performSearch(); }} title="Use Regular Expression (Alt+R)">.*</div>
                            </div>
                        </div>
                        
                        ${this.replaceExpanded ? html`
                            <div class="input-box replace">
                                <input 
                                    type="text" 
                                    placeholder="Replace" 
                                    .value=${this.replaceString}
                                    @input=${(e: Event) => this.replaceString = (e.target as HTMLInputElement).value}
                                    @keydown=${this.handleReplaceKeydown}
                                />
                                <div class="input-actions">
                                    <div class="action-btn" @click=${() => this.performSearch(true)} title="Replace All (Ctrl+Alt+Enter)">
                                        ${unsafeSVG(icon_replace_all)}
                                    </div>
                                </div>
                            </div>
                        ` : ''}
                    </div>
                </div>
            </div>
            
            <div class="results-container">
                ${this.isSearching ? html`<div class="no-results">Searching...</div>` : ''}
                ${!this.isSearching && !(this.results.files?.length) && !(this.results.folders?.length) && !(this.results.words?.length) && this.query ? html`<div class="no-results">No results found.</div>` : ''}
                
                ${this.results.folders && this.results.folders.length > 0 ? html`
                    <div class="file-group">
                        <div class="file-header">FOLDERS <span style="opacity:0.5; font-size:9px; margin-left:auto">${this.results.folders.length}</span></div>
                        ${this.results.folders.map(f => html`
                            <div class="result-item" @click=${() => this.dispatchEvent(new CustomEvent('open-file', { detail: { path: f.startsWith('file:///') ? f : 'file:///' + f, name: f.split(/[\\/]/).pop(), line: 1 }, bubbles: true, composed: true }))}>
                                <span class="result-text">${this.renderHighlightedText(f.split(/[\\/]/).pop() || f)}</span>
                            </div>
                        `)}
                    </div>
                ` : ''}

                ${this.results.files && this.results.files.length > 0 ? html`
                    <div class="file-group">
                        <div class="file-header">FILES <span style="opacity:0.5; font-size:9px; margin-left:auto">${this.results.files.length}</span></div>
                        ${this.results.files.map(f => html`
                            <div class="result-item" @click=${() => this.dispatchEvent(new CustomEvent('open-file', { detail: { path: f.startsWith('file:///') ? f : 'file:///' + f, name: f.split(/[\\/]/).pop(), line: 1 }, bubbles: true, composed: true }))}>
                                <span class="result-text">${this.renderHighlightedText(f.split(/[\\/]/).pop() || f)}</span>
                            </div>
                        `)}
                    </div>
                ` : ''}

                ${Object.entries(grouped).length > 0 ? html`
                    <div class="file-group">
                        <div class="file-header" style="background: transparent; color: #7aa2f7; border-bottom: 1px solid #292e42; padding-left: 8px;">WORDS</div>
                    </div>
                ` : ''}
                
                ${Object.entries(grouped).map(([file, items]) => html`
                    <div class="file-group">
                        <div class="file-header">${file.split(/[\\/]/).pop()} <span style="opacity:0.5; font-size:9px; margin-left:auto">${items.length}</span></div>
                        ${items.map(res => html`
                            <div class="result-item" @click=${() => this.handleResultClick(res)}>
                                <span class="line-num">${res.line}</span>
                                <span class="result-text">${this.renderHighlightedText(res.text.trim())}</span>
                            </div>
                        `)}
                    </div>
                `)}
            </div>
        `;
    }
}
