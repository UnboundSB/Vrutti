import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_error, icon_warning } from './codicons';

interface Problem {
    severity: 'error' | 'warning' | 'info';
    message: string;
    file: string;
    line: number;
    col: number;
}

interface DiagnosticEntry {
    uri: string;
    diagnostics: Array<{
        range: {
            start: { line: number, character: number },
            end: { line: number, character: number }
        },
        message: string,
        severity: number,
        code?: string,
        source?: string
    }>;
}

@customElement('vrutti-problems-view')
export class VruttiProblemsView extends LitElement {
    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            overflow-y: auto;
            color: var(--vrutti-text, #cccccc);
            font-size: 13px;
            font-family: var(--vrutti-ui-font, 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif);
        }
        .header {
            padding: 8px 15px;
            color: var(--vrutti-text-bright, #ffffff);
            display: flex;
            align-items: center;
        }
        .file-group {
            margin-bottom: 5px;
        }
        .file-header {
            padding: 4px 15px;
            font-weight: bold;
            display: flex;
            align-items: center;
            background-color: rgba(255, 255, 255, 0.05);
            cursor: pointer;
        }
        .problem-item {
            display: flex;
            align-items: flex-start;
            padding: 4px 15px 4px 35px;
            cursor: pointer;
        }
        .problem-item:hover {
            background-color: var(--vrutti-list-hover-bg, rgba(255, 255, 255, 0.1));
        }
        .icon {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            width: 16px;
            height: 16px;
            margin-right: 6px;
            flex-shrink: 0;
            margin-top: 2px;
        }
        .icon.error { color: #f14c4c; }
        .icon.warning { color: #cca700; }
        .icon.info { color: #3794ff; }
        .message {
            flex: 1;
            word-break: break-word;
        }
        .location {
            color: var(--vrutti-text-dim, #888888);
            margin-left: 8px;
            flex-shrink: 0;
        }
        .empty-state {
            padding: 15px;
            color: var(--vrutti-text-dim, #888888);
            text-align: center;
        }
    `;

    // Map of collectionName -> uri -> Problem[]
    private _collections: Map<string, Map<string, Problem[]>> = new Map();

    @state() private problems: Problem[] = [];

    constructor() {
        super();
        window.addEventListener('vrutti-ipc', this._handleIpc as EventListener);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('vrutti-ipc', this._handleIpc as EventListener);
    }

    private _updateProblemsList() {
        const allProblems: Problem[] = [];
        for (const uriMap of this._collections.values()) {
            for (const probs of uriMap.values()) {
                allProblems.push(...probs);
            }
        }
        this.problems = allProblems;
    }

    private _mapSeverity(sev: number): 'error' | 'warning' | 'info' {
        switch (sev) {
            case 0: return 'error'; // Error
            case 1: return 'warning'; // Warning
            case 2: return 'info'; // Information
            case 3: return 'info'; // Hint
            default: return 'info';
        }
    }

    private _handleIpc = (e: CustomEvent) => {
        const msg = e.detail;
        
        // Handle VS Code Extension Host diagnostic events
        if (msg.method === 'languages/diagnostics') {
            const collectionName = msg.params?.collection;
            const entries = msg.params?.entries as DiagnosticEntry[];
            
            if (collectionName && entries) {
                let uriMap = this._collections.get(collectionName);
                if (!uriMap) {
                    uriMap = new Map<string, Problem[]>();
                    this._collections.set(collectionName, uriMap);
                }
                
                for (const entry of entries) {
                    // Extract file path from uri (e.g., file:///c:/...)
                    let file = entry.uri;
                    if (file.startsWith('file:///')) {
                        file = file.substring(8);
                    }
                    
                    const fileProblems: Problem[] = (entry.diagnostics || []).map(d => ({
                        severity: this._mapSeverity(d.severity),
                        message: d.message,
                        file: file,
                        line: (d.range?.start?.line || 0) + 1, // 1-indexed for display
                        col: (d.range?.start?.character || 0) + 1
                    }));
                    
                    uriMap.set(entry.uri, fileProblems);
                }
                this._updateProblemsList();
            }
        } else if (msg.method === 'languages/diagnostics/clear') {
            const collectionName = msg.params?.collection;
            if (collectionName) {
                this._collections.delete(collectionName);
                this._updateProblemsList();
            }
        } else if (msg.method === 'languages/diagnostics/delete') {
            const collectionName = msg.params?.collection;
            const uri = msg.params?.uri;
            if (collectionName && uri) {
                const uriMap = this._collections.get(collectionName);
                if (uriMap) {
                    uriMap.delete(uri);
                    this._updateProblemsList();
                }
            }
        } else if (msg.method === 'diagnostics/update') {
            // Support legacy/custom diagnostics/update format if needed
            this.problems = msg.params?.diagnostics || [];
        }
    };

    private _openFile(problem: Problem) {
        window.dispatchEvent(new CustomEvent('open-file', {
            detail: {
                filePath: problem.file,
                line: problem.line,
                col: problem.col
            }
        }));
    }

    private _getIcon(severity: string) {
        if (severity === 'error') return unsafeSVG(icon_error);
        if (severity === 'warning') return unsafeSVG(icon_warning);
        return html`<span style="width:16px;height:16px;display:inline-block;border-radius:50%;background:#3794ff;"></span>`; // fallback info
    }

    render() {
        if (this.problems.length === 0) {
            return html`
                <div class="empty-state">
                    No problems have been detected in the workspace.
                </div>
            `;
        }

        // Group by file
        const groups: Record<string, Problem[]> = {};
        for (const p of this.problems) {
            if (!groups[p.file]) groups[p.file] = [];
            groups[p.file].push(p);
        }

        return html`
            <div class="header">
                ${this.problems.length} problems
            </div>
            ${Object.entries(groups).map(([file, fileProblems]) => html`
                <div class="file-group">
                    <div class="file-header">${file}</div>
                    ${fileProblems.map(p => html`
                        <div class="problem-item" @click=${() => this._openFile(p)}>
                            <div class="icon ${p.severity}">
                                ${this._getIcon(p.severity)}
                            </div>
                            <div class="message">${p.message}</div>
                            <div class="location">[${p.line}, ${p.col}]</div>
                        </div>
                    `)}
                </div>
            `)}
        `;
    }
}
