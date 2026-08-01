import { LitElement, html, css, svg } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { globalHoverStyle } from '../../shared-styles';

interface Commit {
    hash: string;
    parents: string[];
    refs: string;
    subject: string;
    date: string;
    author: string;
}

interface GraphRow {
    commit: Commit;
    oldColumns: string[];
    newColumns: string[];
    colIndex: number;
}

const colors = [
    '#7aa2f7', '#9ece6a', '#e0af68', '#f7768e', '#bb9af7',
    '#7dcfff', '#ff9e64', '#1abc9c', '#e74c3c', '#9b59b6'
];

@customElement('vrutti-git-graph')
export class VruttiGitGraph extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: flex;
            flex-direction: column;
            position: absolute;
            width: 800px;
            height: 600px;
            background-color: rgba(26, 27, 38, 0.95);
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
            color: var(--vrutti-text, #c0caf5);
            font-family: var(--vrutti-font, 'Segoe UI', sans-serif);
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
            border: 1px solid var(--vrutti-surface-border, #2a2e42);
            border-radius: 8px;
            overflow: hidden;
            z-index: 10000;
        }

        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 16px;
            background: rgba(30, 30, 46, 0.8);
            border-bottom: 1px solid var(--vrutti-surface-border, #2a2e42);
            cursor: grab;
            user-select: none;
        }

        .header:active {
            cursor: grabbing;
        }

        .title {
            font-size: 14px;
            font-weight: 600;
            color: #c0caf5;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .search-bar {
            display: flex;
            align-items: center;
            background: var(--vrutti-bg, #16161e);
            border: 1px solid var(--vrutti-surface-border, #2a2e42);
            border-radius: 4px;
            padding: 4px 8px;
            width: 300px;
        }

        .search-bar input {
            background: transparent;
            border: none;
            color: #c0caf5;
            outline: none;
            width: 100%;
            font-family: inherit;
        }

        .close-btn {
            background: transparent;
            border: none;
            color: #565f89;
            cursor: pointer;
            padding: 4px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 4px;
        }

        .close-btn:hover {
            color: #f7768e;
            background: rgba(247, 118, 142, 0.1);
        }

        .content {
            display: flex;
            flex: 1;
            overflow: hidden;
        }

        .list-container {
            flex: 1;
            overflow-y: auto;
            position: relative;
        }

        .commit-row {
            display: flex;
            height: 28px;
            align-items: center;
            cursor: pointer;
            position: relative;
        }

        .commit-row:hover {
            background-color: rgba(255, 255, 255, 0.03);
        }

        .commit-row.selected {
            background-color: rgba(122, 162, 247, 0.1);
        }

        .graph-col {
            position: relative;
            height: 28px;
            flex-shrink: 0;
        }

        .commit-info {
            display: flex;
            align-items: center;
            flex: 1;
            padding-left: 8px;
            overflow: hidden;
            white-space: nowrap;
            gap: 8px;
        }

        .commit-msg {
            color: #c0caf5;
            font-size: 13px;
            overflow: hidden;
            text-overflow: ellipsis;
            flex: 1;
        }

        .commit-hash {
            color: #e0af68;
            font-family: monospace;
            font-size: 12px;
        }

        .commit-author {
            color: #7aa2f7;
            font-size: 12px;
            width: 100px;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .commit-date {
            color: #565f89;
            font-size: 12px;
            width: 80px;
        }

        .ref-tag {
            background: rgba(158, 206, 106, 0.2);
            color: #9ece6a;
            border: 1px solid rgba(158, 206, 106, 0.4);
            border-radius: 12px;
            padding: 0 6px;
            font-size: 10px;
            font-weight: 600;
        }

        .detail-panel {
            width: 300px;
            border-left: 1px solid var(--vrutti-surface-border, #2a2e42);
            background: rgba(26, 27, 38, 0.5);
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 12px;
            overflow-y: auto;
        }

        .detail-row {
            display: flex;
            flex-direction: column;
            gap: 4px;
        }

        .detail-label {
            font-size: 11px;
            text-transform: uppercase;
            color: #565f89;
            font-weight: 600;
        }

        .detail-value {
            font-size: 13px;
            color: #c0caf5;
            word-wrap: break-word;
        }

        .detail-value.monospace {
            font-family: monospace;
            color: #e0af68;
        }

        ::-webkit-scrollbar {
            width: 10px;
            height: 10px;
            background: transparent;
        }
        ::-webkit-scrollbar-thumb {
            background: transparent;
            border-radius: 5px;
        }
        :hover::-webkit-scrollbar-thumb {
            background-color: rgba(122, 162, 247, 0.4);
        }
    `];

    @state() private commits: Commit[] = [];
    @state() private graphRows: GraphRow[] = [];
    @state() private searchQuery: string = '';
    @state() private selectedCommit: Commit | null = null;
    @state() private maxCols: number = 0;

    @state() private pos = { x: 100, y: 100 };
    private isDragging = false;
    private dragStart = { x: 0, y: 0 };

    async firstUpdated() {
        await this.loadHistory();
    }

    private startDrag = (e: MouseEvent) => {
        if ((e.target as HTMLElement).closest('.search-bar') || (e.target as HTMLElement).closest('.close-btn')) return;
        this.isDragging = true;
        this.dragStart = { x: e.clientX - this.pos.x, y: e.clientY - this.pos.y };
        window.addEventListener('mousemove', this.doDrag);
        window.addEventListener('mouseup', this.stopDrag);
    };

    private doDrag = (e: MouseEvent) => {
        if (!this.isDragging) return;
        this.pos = { x: e.clientX - this.dragStart.x, y: e.clientY - this.dragStart.y };
    };

    private stopDrag = () => {
        this.isDragging = false;
        window.removeEventListener('mousemove', this.doDrag);
        window.removeEventListener('mouseup', this.stopDrag);
    };

    private async runGit(args: string): Promise<{stdout: string, exitCode: number}> {
        const wp = (window as any).currentWorkspace || '';
        if (!wp) return {stdout: '', exitCode: -1};
        try {
            const res = await (window as any).vruttiGitCommand(wp, `git ${args}`);
            return JSON.parse(res);
        } catch (e) {
            console.error("Git error:", e);
            return {stdout: '', exitCode: -1};
        }
    }

    private async loadHistory() {
        const res = await this.runGit('log --all --date-order --format="%H@@@%P@@@%d@@@%s@@@%cd@@@%an" --date=short');
        if (res.exitCode !== 0) return;

        const lines = res.stdout.split('\n');
        const parsed: Commit[] = [];

        for (const line of lines) {
            if (!line.trim()) continue;
            const parts = line.split('@@@');
            if (parts.length >= 6) {
                const parents = parts[1].trim() ? parts[1].trim().split(' ') : [];
                parsed.push({
                    hash: parts[0].trim(),
                    parents: parents,
                    refs: parts[2].trim().replace(/^\s*\(|\)\s*$/g, ''),
                    subject: parts.slice(3, -2).join('@@@').trim(), 
                    date: parts[parts.length - 2].trim(),
                    author: parts[parts.length - 1].trim()
                });
            }
        }

        this.commits = parsed;
        this.computeGraph();
    }

    private computeGraph() {
        let columns: string[] = [];
        const rows: GraphRow[] = [];
        let maxCols = 0;

        for (const c of this.commits) {
            let colIndex = columns.indexOf(c.hash);
            if (colIndex === -1) {
                colIndex = columns.findIndex(h => !h);
                if (colIndex === -1) {
                    colIndex = columns.length;
                    columns.push(c.hash);
                } else {
                    columns[colIndex] = c.hash;
                }
            }

            const oldColumns = [...columns];
            const nextColumns = [...columns];

            if (c.parents.length > 0) {
                nextColumns[colIndex] = c.parents[0];
                for (let i = 1; i < c.parents.length; i++) {
                    const p = c.parents[i];
                    if (!nextColumns.includes(p)) {
                        const emptyIdx = nextColumns.findIndex(h => !h);
                        if (emptyIdx === -1) nextColumns.push(p);
                        else nextColumns[emptyIdx] = p;
                    }
                }
            } else {
                nextColumns[colIndex] = '';
            }

            while(nextColumns.length > 0 && !nextColumns[nextColumns.length - 1]) {
                nextColumns.pop();
            }

            rows.push({
                commit: c,
                oldColumns,
                newColumns: nextColumns,
                colIndex
            });

            columns = nextColumns;
            if (columns.length > maxCols) maxCols = columns.length;
        }

        this.graphRows = rows;
        this.maxCols = maxCols;
    }

    private getColor(idx: number) {
        return colors[idx % colors.length];
    }

    private renderSvgLines(row: GraphRow) {
        const colWidth = 14;
        const rowHeight = 28;
        const radius = 4;
        
        const lines = [];

        for (let i = 0; i < row.oldColumns.length; i++) {
            const hash = row.oldColumns[i];
            if (!hash) continue;

            if (i === row.colIndex) {
                for (let j = 0; j < row.commit.parents.length; j++) {
                    const p = row.commit.parents[j];
                    const targetIdx = row.newColumns.indexOf(p);
                    if (targetIdx !== -1) {
                        const x1 = i * colWidth + colWidth/2;
                        const y1 = rowHeight / 2;
                        const x2 = targetIdx * colWidth + colWidth/2;
                        const y2 = rowHeight;
                        
                        const color = this.getColor(i);
                        
                        if (x1 === x2) {
                            lines.push(svg`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${color}" stroke-width="2" />`);
                        } else {
                            lines.push(svg`<path d="M ${x1} ${y1} C ${x1} ${y1+10}, ${x2} ${y2-10}, ${x2} ${y2}" fill="none" stroke="${color}" stroke-width="2" />`);
                        }
                    }
                }
            } else {
                const targetIdx = row.newColumns.indexOf(hash);
                if (targetIdx !== -1) {
                    const x1 = i * colWidth + colWidth/2;
                    const y1 = 0;
                    const x2 = targetIdx * colWidth + colWidth/2;
                    const y2 = rowHeight;
                    
                    const color = this.getColor(i);
                    
                    if (x1 === x2) {
                        lines.push(svg`<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${color}" stroke-width="2" />`);
                    } else {
                        lines.push(svg`<path d="M ${x1} ${y1} C ${x1} ${y1+10}, ${x2} ${y2-10}, ${x2} ${y2}" fill="none" stroke="${color}" stroke-width="2" />`);
                    }
                }
            }
        }

        // Draw incoming lines for passthrough
        for (let i = 0; i < row.oldColumns.length; i++) {
            if (!row.oldColumns[i] || i === row.colIndex) continue;
            const x = i * colWidth + colWidth/2;
            lines.push(svg`<line x1="${x}" y1="0" x2="${x}" y2="${rowHeight/2}" stroke="${this.getColor(i)}" stroke-width="2" />`);
        }

        // Draw node
        const cx = row.colIndex * colWidth + colWidth/2;
        const cy = rowHeight / 2;
        lines.push(svg`<circle cx="${cx}" cy="${cy}" r="${radius}" fill="${this.getColor(row.colIndex)}" />`);

        return svg`
            <svg width="${this.maxCols * colWidth}" height="${rowHeight}">
                ${lines}
            </svg>
        `;
    }

    private closeWindow() {
        this.dispatchEvent(new CustomEvent('close-git-graph', { bubbles: true, composed: true }));
    }

    render() {
        this.style.left = `${this.pos.x}px`;
        this.style.top = `${this.pos.y}px`;

        const filteredRows = this.searchQuery 
            ? this.graphRows.filter(r => {
                const q = this.searchQuery.toLowerCase();
                return r.commit.hash.toLowerCase().includes(q) ||
                       r.commit.subject.toLowerCase().includes(q) ||
                       r.commit.author.toLowerCase().includes(q) ||
                       r.commit.date.includes(q) ||
                       r.commit.refs.toLowerCase().includes(q);
              })
            : this.graphRows;

        return html`
            <div class="header" @mousedown=${this.startDrag}>
                <div class="title">
                    <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M4 2a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 2Zm10 12a2 2 0 1 1-1.85-2.75l-1.01-.505a.75.75 0 0 1 .673-1.343l1.01.505A2 2 0 0 1 14 14ZM4 10a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 10Zm5-5a2 2 0 1 1-1.85 2.75l-3.02 1.51a.75.75 0 0 1-.673-1.343l3.02-1.51A2 2 0 0 1 9 5Z"/></svg>
                    Git History
                </div>
                <div class="search-bar">
                    <input 
                        type="text" 
                        placeholder="Search commits by message, hash, author..." 
                        .value=${this.searchQuery}
                        @input=${(e: any) => this.searchQuery = e.target.value}
                    />
                </div>
                <button class="close-btn" @click=${this.closeWindow}>
                    <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M3.72 3.72a.75.75 0 0 1 1.06 0L8 6.94l3.22-3.22a.75.75 0 1 1 1.06 1.06L9.06 8l3.22 3.22a.75.75 0 1 1-1.06 1.06L8 9.06l-3.22 3.22a.75.75 0 0 1-1.06-1.06L6.94 8 3.72 4.78a.75.75 0 0 1 0-1.06Z"/></svg>
                </button>
            </div>
            <div class="content">
                <div class="list-container">
                    ${filteredRows.map(row => html`
                        <div class="commit-row ${this.selectedCommit === row.commit ? 'selected' : ''}" @click=${() => this.selectedCommit = row.commit}>
                            <div class="graph-col" style="width: ${this.maxCols * 14 + 8}px;">
                                ${this.searchQuery ? '' : this.renderSvgLines(row)}
                            </div>
                            <div class="commit-info">
                                <span class="commit-hash">${row.commit.hash.substring(0, 7)}</span>
                                <span class="commit-msg">
                                    ${row.commit.refs ? row.commit.refs.split(',').map(r => html`<span class="ref-tag">${r.trim()}</span>`) : ''}
                                    ${row.commit.subject}
                                </span>
                                <span class="commit-author" title="${row.commit.author}">${row.commit.author}</span>
                                <span class="commit-date">${row.commit.date}</span>
                            </div>
                        </div>
                    `)}
                </div>
                ${this.selectedCommit ? html`
                    <div class="detail-panel">
                        <div class="detail-row">
                            <span class="detail-label">Commit</span>
                            <span class="detail-value monospace">${this.selectedCommit.hash}</span>
                        </div>
                        <div class="detail-row">
                            <span class="detail-label">Author</span>
                            <span class="detail-value">${this.selectedCommit.author}</span>
                        </div>
                        <div class="detail-row">
                            <span class="detail-label">Date</span>
                            <span class="detail-value">${this.selectedCommit.date}</span>
                        </div>
                        ${this.selectedCommit.parents.length > 0 ? html`
                            <div class="detail-row">
                                <span class="detail-label">Parents</span>
                                <span class="detail-value monospace">${this.selectedCommit.parents.map(p => p.substring(0,7)).join(', ')}</span>
                            </div>
                        ` : ''}
                        <div class="detail-row" style="margin-top: 12px;">
                            <span class="detail-label">Message</span>
                            <span class="detail-value" style="white-space: pre-wrap;">${this.selectedCommit.subject}</span>
                        </div>
                    </div>
                ` : ''}
            </div>
        `;
    }
}
