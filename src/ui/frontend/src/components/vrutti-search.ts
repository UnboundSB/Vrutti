import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { unsafeHTML } from 'lit/directives/unsafe-html.js';
import { globalHoverStyle } from '../shared-styles';
import { icon_chevron_down } from './codicons';
import { registry } from '../core/Registry';

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
            color: var(--vrutti-text-muted, #636b95);
            margin-top: 2px;
        }

        .toggle-chevron:hover {
            color: var(--vrutti-text, #a6accd);
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
            background: var(--vrutti-input-bg, #1a1b26);
            border: 1px solid var(--vrutti-input-border, #3b4261);
            border-radius: 4px;
            padding-right: 70px;
        }
        
        .input-box.replace {
            padding-right: 24px;
        }

        .input-box:focus-within {
            border-color: var(--vrutti-focus-border, #7aa2f7);
        }

        input {
            width: 100%;
            background: transparent;
            border: none;
            color: var(--vrutti-input-text, #c0caf5);
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
            color: var(--vrutti-text-muted, #636b95);
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
            background: var(--vrutti-list-hover-bg, rgba(255, 255, 255, 0.1));
            color: var(--vrutti-text, #a6accd);
        }

        .action-btn.active {
            background: var(--vrutti-list-active-bg, rgba(122, 162, 247, 0.2));
            color: var(--vrutti-accent, #7aa2f7);
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
            color: var(--vrutti-text-secondary, #a9b1d6);
            background: var(--vrutti-side-bg, #1a1b26);
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
            color: var(--vrutti-text, #c0caf5);
            transition: background 0.1s;
            display: flex;
            gap: 8px;
        }

        .result-item:hover {
            background: var(--vrutti-list-hover-bg, #292e42);
        }

        .line-num {
            color: var(--vrutti-text-muted, #565f89);
            min-width: 24px;
            text-align: right;
        }

        .no-results {
            padding: 12px;
            color: var(--vrutti-text-muted, #565f89);
            text-align: center;
        }

        .highlight {
            background-color: var(--vrutti-find-match-bg, rgba(122, 162, 247, 0.4));
            color: var(--vrutti-find-match-text, #ffffff);
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
    @state() private includes = '';
    @state() private excludes = '';
    @state() private directory = '.';
    @state() private results: { files: string[], folders: string[], words: SearchResult[] } = { files: [], folders: [], words: [] };
    @state() private isSearching = false;
    
    @state() private replaceExpanded = false;
    @state() private detailsExpanded = false;
    @state() private matchCase = false;
    @state() private wholeWord = false;
    @state() private useRegex = false;
    
    @state() private expandedFiles: Set<string> = new Set();
    

    private searchStartTime: number = 0;

    private handleIpc = (e: CustomEvent) => {
        if (e.detail && e.detail.method === 'search/results') {
            const params = e.detail.params;
            if (params && params.searchId === this.currentSearchId) {
                const t0 = performance.now();
                this.results = params.results;
                this.expandedFiles = new Set(Object.keys(this.groupResultsByFile())); // Expand all by default
                this.isSearching = false;
                console.log(`[SearchProfile] Final search/results received in ${performance.now() - this.searchStartTime} ms since request. Processing took ${performance.now() - t0} ms.`);
            }
        } else if (e.detail && e.detail.method === 'search/results/partial') {
            const params = e.detail.params;
            if (params && params.searchId === this.currentSearchId) {
                this.results = {
                    files: [...(this.results.files || []), ...(params.results.files || [])],
                    folders: [...(this.results.folders || []), ...(params.results.folders || [])],
                    words: [...(this.results.words || []), ...(params.results.words || [])]
                };
                this.expandedFiles = new Set([...this.expandedFiles, ...Object.keys(this.groupResultsByFile())]);
            }
        }
    };

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

    private toggleFileExpansion(file: string) {
        const newSet = new Set(this.expandedFiles);
        if (newSet.has(file)) {
            newSet.delete(file);
        } else {
            newSet.add(file);
        }
        this.expandedFiles = newSet;
    }

    private searchTimeout?: number;
    private currentSearchId: number = 0;

    private performSearch = async (replace: boolean = false) => {
        let actualDir = this.directory;
        if (!actualDir || actualDir === '.') {
            actualDir = (window as any).currentWorkspace || '.';
            if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
            else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);
        }
        
        if (!actualDir) return;
        
        if (replace && this.query && this.replaceString && this.results.words?.length > 0) {
            // ... (replace logic untouched)
        }

        if (this.searchTimeout) {
            window.clearTimeout(this.searchTimeout);
        }

        if (!this.query.trim()) {
            // Trigger file search
            this.isSearching = true;
            this.currentSearchId++;
            this.results = { files: [], folders: [], words: [] };
            const searchId = this.currentSearchId;
            try {
                if ((window as any).vruttiSearchAsync) {
                    (window as any).vruttiSearchAsync({ directory: actualDir, query: "", includes: this.includes, excludes: this.excludes, searchId });
                }
            } catch (err) {
                if (searchId === this.currentSearchId) {
                    console.error("Search failed", err);
                    this.results = { files: [], folders: [], words: [] };
                    this.dispatchEvent(new CustomEvent('search-error', { detail: { message: "Search failed due to an error." }, bubbles: true, composed: true }));
                    this.isSearching = false;
                }
            }
            return;
        }

        this.searchTimeout = window.setTimeout(async () => {
            this.isSearching = true;
            this.currentSearchId++;
            this.results = { files: [], folders: [], words: [] };
            const searchId = this.currentSearchId;
            try {
                this.searchStartTime = performance.now();
                console.log(`[SearchProfile] Sending search IPC...`);
                if ((window as any).vruttiSearchAsync) {
                    (window as any).vruttiSearchAsync({
                        directory: actualDir,
                        query: this.query,
                        includes: this.includes,
                        excludes: this.excludes,
                        matchCase: this.matchCase,
                        wholeWord: this.wholeWord,
                        useRegex: this.useRegex,
                        searchId
                    });
                }
            } catch (err) {
                if (searchId === this.currentSearchId) {
                    console.error("Search failed", err);
                    this.results = { files: [], folders: [], words: [] };
                    this.dispatchEvent(new CustomEvent('search-error', { detail: { message: "Search failed due to an error." }, bubbles: true, composed: true }));
                    this.isSearching = false;
                }
            }
        }, 150);
    };

    private handleToggleMatchCase = () => { this.matchCase = !this.matchCase; this.performSearch(); };
    private handleToggleWholeWord = () => { this.wholeWord = !this.wholeWord; this.performSearch(); };
    private handleToggleRegex = () => { this.useRegex = !this.useRegex; this.performSearch(); };
    private handleReplaceAll = () => { this.performSearch(true); };

    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
        window.addEventListener('search-toggle-match-case', this.handleToggleMatchCase);
        window.addEventListener('search-toggle-whole-word', this.handleToggleWholeWord);
        window.addEventListener('search-toggle-regex', this.handleToggleRegex);
        window.addEventListener('search-replace-all', this.handleReplaceAll);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
        window.removeEventListener('search-toggle-match-case', this.handleToggleMatchCase);
        window.removeEventListener('search-toggle-whole-word', this.handleToggleWholeWord);
        window.removeEventListener('search-toggle-regex', this.handleToggleRegex);
        window.removeEventListener('search-replace-all', this.handleReplaceAll);
        if (this.searchTimeout) window.clearTimeout(this.searchTimeout);
    }
    
    private isActionActive(command?: string) {
        if (command === 'search.toggleMatchCase') return this.matchCase;
        if (command === 'search.toggleWholeWord') return this.wholeWord;
        if (command === 'search.toggleRegex') return this.useRegex;
        return false;
    }

    // @ts-expect-error unused method
    private handleSearchChunk = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (detail && detail.searchId === this.currentSearchId) {
            const t0 = performance.now();
            this.isSearching = false;
            
            const newFiles = detail.files || [];
            const newWords = detail.words || [];
            
            this.results.files = [...(this.results.files || []), ...newFiles];
            this.results.words = [...(this.results.words || []), ...newWords];
            
            const newSet = new Set(this.expandedFiles);
            const groups = this.groupResultsByFile();
            for (const file of Object.keys(groups)) {
                if (!newSet.has(file)) {
                    newSet.add(file);
                }
            }
            this.expandedFiles = newSet;
            
            this.requestUpdate();
            const t1 = performance.now();
            console.log(`[SearchProfile] Processed chunk of ${newWords.length} words in ${t1 - t0} ms. Total time since search start: ${t1 - this.searchStartTime} ms.`);
        }
    };

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
                                ${(registry.getMenu('search/inputActions')?.items || []).map(item => html`
                                    <div class="action-btn ${this.isActionActive(item.command) ? 'active' : ''}" 
                                         @click=${() => registry.executeCommand(item.command || '')} 
                                         title="${item.label}">
                                        ${item.iconContent ? (item.iconContent.startsWith('<svg') ? unsafeHTML(item.iconContent) : item.iconContent) : ''}
                                    </div>
                                `)}
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
                                    ${(registry.getMenu('search/replaceActions')?.items || []).map(item => html`
                                        <div class="action-btn" @click=${() => registry.executeCommand(item.command || '')} title="${item.label}">
                                            ${item.iconContent ? (item.iconContent.startsWith('<svg') ? unsafeHTML(item.iconContent) : item.iconContent) : ''}
                                        </div>
                                    `)}
                                </div>
                            </div>
                        ` : ''}
                    </div>
                </div>
                
                <div style="display:flex; justify-content: flex-end; padding-right: 4px; margin-top: 2px;">
                    <div class="action-btn ${this.detailsExpanded ? 'active' : ''}" style="width: 24px" @click=${() => this.detailsExpanded = !this.detailsExpanded} title="Toggle Search Details">...</div>
                </div>
                
                ${this.detailsExpanded ? html`
                    <div style="display:flex; flex-direction:column; gap:6px; margin-top: 4px; padding-left: 24px;">
                        <div>
                            <div style="font-size:10px; opacity:0.7; margin-bottom:2px;">files to include</div>
                            <div class="input-box replace">
                                <input type="text" placeholder="e.g. *.ts, src/**/include" .value=${this.includes} @input=${(e: Event) => this.includes = (e.target as HTMLInputElement).value} @keydown=${this.handleQueryKeydown} />
                            </div>
                        </div>
                        <div>
                            <div style="font-size:10px; opacity:0.7; margin-bottom:2px;">files to exclude</div>
                            <div class="input-box replace">
                                <input type="text" placeholder="e.g. *.js, node_modules" .value=${this.excludes} @input=${(e: Event) => this.excludes = (e.target as HTMLInputElement).value} @keydown=${this.handleQueryKeydown} />
                            </div>
                        </div>
                    </div>
                ` : ''}
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
                        <div class="file-header" style="background: transparent; color: var(--vrutti-accent, #7aa2f7); border-bottom: 1px solid var(--vrutti-border, #292e42); padding-left: 8px;">WORDS</div>
                    </div>
                ` : ''}
                
                ${Object.entries(grouped).slice(0, 50).map(([file, items]) => html`
                    <div class="file-group">
                        <div class="file-header" style="cursor:pointer;" @click=${() => this.toggleFileExpansion(file)}>
                            <div class="toggle-chevron ${this.expandedFiles.has(file) ? 'expanded' : ''}" style="width:14px; height:14px; margin-right:4px;">
                                ${unsafeSVG(icon_chevron_down)}
                            </div>
                            ${file.split(/[\\/]/).pop()}
                            <span style="opacity:0.5; font-size:10px; margin-left:8px; font-weight:normal;">${file}</span>
                            <span style="opacity:0.6; font-size:10px; margin-left:auto; background:rgba(255,255,255,0.1); padding:0 6px; border-radius:10px;">${items.length}</span>
                        </div>
                        ${this.expandedFiles.has(file) ? items.slice(0, 100).map(res => html`
                            <div class="result-item" @click=${() => this.handleResultClick(res)}>
                                <span class="line-num">${res.line}</span>
                                <span class="result-text">${this.renderHighlightedText(res.text.trim())}</span>
                            </div>
                        `) : ''}
                        ${this.expandedFiles.has(file) && items.length > 100 ? html`
                            <div class="result-item" style="opacity: 0.5; justify-content: center;">
                                <span>... and ${items.length - 100} more matches in this file</span>
                            </div>
                        ` : ''}
                    </div>
                `)}
                ${Object.keys(grouped).length > 50 ? html`
                    <div style="padding: 12px; text-align: center; color: var(--vrutti-warning, #ff9e64); font-size: 11px; font-weight: 600;">
                        ⚠️ Showing first 50 files to keep the editor fast. <br/> Please narrow your search.
                    </div>
                ` : ''}
            </div>
        `;
    }
}
