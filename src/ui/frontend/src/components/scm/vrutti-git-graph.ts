import { LitElement, html, css, svg } from 'lit';
import { customElement, state, query } from 'lit/decorators.js';
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
    '#ff4757', '#a29bfe', '#74b9ff', '#55efc4', '#ffeaa7',
    '#fab1a0', '#fd79a8', '#81ecec', '#00b894', '#6c5ce7'
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

        .canvas-scroll-view {
            flex: 1;
            overflow: auto;
            position: relative;
            background: rgba(22, 22, 30, 0.4);
        }
        
        .canvas-container {
            position: relative;
        }

        .node-label {
            position: absolute;
            transform: translateX(-50%);
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 4px;
            pointer-events: none;
            opacity: 0.8;
            transition: opacity 0.2s;
        }

        .node-label:hover, .node-label.selected {
            opacity: 1;
            z-index: 10;
        }

        .node-msg {
            font-size: 11px;
            color: #c0caf5;
            white-space: nowrap;
            background: rgba(26, 27, 38, 0.8);
            padding: 2px 6px;
            border-radius: 4px;
            border: 1px solid transparent;
            pointer-events: auto;
            cursor: pointer;
        }
        
        .node-label.selected .node-msg {
            border-color: #7aa2f7;
            color: #7aa2f7;
            background: rgba(26, 27, 38, 0.95);
        }

        .ref-tag {
            background: rgba(158, 206, 106, 0.2);
            color: #9ece6a;
            border: 1px solid rgba(158, 206, 106, 0.4);
            border-radius: 12px;
            padding: 0 6px;
            font-size: 9px;
            font-weight: 600;
            pointer-events: auto;
            white-space: nowrap;
        }
        
        .commit-node {
            transition: all 0.2s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            cursor: pointer;
            transform-origin: center;
        }
        
        .commit-node:hover {
            stroke-width: 8;
            transform: scale(1.5);
        }

        .zoom-controls {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-right: 16px;
        }
        
        .zoom-btn {
            background: rgba(255,255,255,0.1);
            border: 1px solid rgba(255,255,255,0.2);
            color: white;
            border-radius: 4px;
            width: 24px;
            height: 24px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            font-size: 14px;
        }
        .zoom-btn:hover {
            background: rgba(255,255,255,0.2);
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
    @state() private errorMsg: string = '';

    @state() private pos = { x: 100, y: 100 };
    @state() private zoom = 1.0;
    @query('.canvas-scroll-view') scrollView!: HTMLElement;
    private hasScrolled = false;

    private handleWheel = (e: WheelEvent) => {
        if (e.ctrlKey || e.metaKey) {
            e.preventDefault();
            this.zoom = Math.max(0.5, Math.min(3.0, this.zoom - e.deltaY * 0.005));
        } else {
            // Translate vertical scroll to horizontal scroll
            if (this.scrollView) {
                e.preventDefault();
                this.scrollView.scrollLeft += e.deltaY;
            }
        }
    };

    updated(_changed: Map<string, any>) {
        if (this.graphRows.length > 0 && !this.hasScrolled && this.scrollView) {
            this.scrollView.scrollLeft = this.scrollView.scrollWidth;
            this.hasScrolled = true;
        }
    }
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
        if (!wp) return {stdout: 'No workspace folder is currently open! Please drag and drop a folder (like vrutti_ide) into the editor to set the workspace.', exitCode: -1};
        try {
            const res = await (window as any).vruttiGitCommand(wp, `git ${args}`);
            return typeof res === 'string' ? JSON.parse(res) : res;
        } catch (e: any) {
            console.error("Git error:", e);
            return {stdout: `Failed to execute git command. Exception: ${e.message}`, exitCode: -1};
        }
    }

    private async loadHistory() {
        this.errorMsg = 'Loading...';
        const res = await this.runGit('log --all --reflog --date-order --format="%H@@@%P@@@%d@@@%s@@@%cd@@@%an" --date=short');
        if (res.exitCode !== 0) {
            this.errorMsg = `Git Error (Code ${res.exitCode}): ${res.stdout}`;
            return;
        }

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

        if (parsed.length === 0) {
            this.errorMsg = `No commits found or parse failed. Raw Output:\n${res.stdout}`;
            return;
        }

        this.errorMsg = '';
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

    private closeWindow() {
        this.dispatchEvent(new CustomEvent('close-git-graph', { bubbles: true, composed: true }));
    }

    render() {
        this.style.left = `${this.pos.x}px`;
        this.style.top = `${this.pos.y}px`;

        const q = this.searchQuery.toLowerCase();
        const isMatch = (c: Commit) => {
            if (!q) return true;
            return c.hash.toLowerCase().includes(q) ||
                   c.subject.toLowerCase().includes(q) ||
                   c.author.toLowerCase().includes(q) ||
                   c.date.includes(q) ||
                   c.refs.toLowerCase().includes(q);
        };

        const nodeSpacingX = 140 * this.zoom;
        const nodeSpacingY = 100 * this.zoom;
        const radius = 10 * this.zoom;
        const padding = 50 * this.zoom;
        const strokeW = 5 * this.zoom;

        const svgWidth = Math.max(800, this.graphRows.length * nodeSpacingX + padding * 2);
        const svgHeight = Math.max(600, (this.maxCols + 1) * nodeSpacingY + padding * 2);

        const nodePositions = new Map<string, {x: number, y: number}>();
        
        let mainLane = 0;
        this.graphRows.forEach((row, i) => {
            if (row.commit.refs.includes('main') || row.commit.refs.includes('master')) {
                mainLane = row.colIndex;
            }
        });

        this.graphRows.forEach((row, i) => {
            const x = (this.graphRows.length - 1 - i) * nodeSpacingX + padding;
            // Center main lane
            const yOffset = (this.maxCols / 2 - mainLane) * nodeSpacingY;
            const y = row.colIndex * nodeSpacingY + padding + (this.maxCols > 0 ? yOffset : 0);
            nodePositions.set(row.commit.hash, {x, y});
        });

        const edgeLines: any[] = [];
        const nodeElements: any[] = [];
        const htmlLabels: any[] = [];

        this.graphRows.forEach((row) => {
            const pos = nodePositions.get(row.commit.hash)!;
            const color = this.getColor(row.colIndex);
            const match = isMatch(row.commit);
            const opacity = (this.searchQuery && !match) ? 0.2 : 1.0;
            
            // Draw edges to parents
            row.commit.parents.forEach(pHash => {
                const pPos = nodePositions.get(pHash);
                if (pPos) {
                    const dx = pos.x - pPos.x;
                    const cpOffset = Math.max(Math.abs(dx) / 2, 40 * this.zoom);
                    edgeLines.push(svg`<path d="M ${pos.x} ${pos.y} C ${pos.x - cpOffset} ${pos.y}, ${pPos.x + cpOffset} ${pPos.y}, ${pPos.x} ${pPos.y}" fill="none" stroke="${color}" stroke-width="${strokeW}" opacity="${opacity}" />`);
                } else {
                    edgeLines.push(svg`<line x1="${pos.x}" y1="${pos.y}" x2="${pos.x - nodeSpacingX/2}" y2="${pos.y}" stroke="${color}" stroke-width="${strokeW}" stroke-dasharray="6" opacity="${opacity}" />`);
                }
            });

            // Draw node
            nodeElements.push(svg`<circle cx="${pos.x}" cy="${pos.y}" r="${radius}" fill="#ffffff" stroke="${color}" stroke-width="${strokeW}" opacity="${opacity}" class="commit-node" @click=${() => this.selectedCommit = row.commit} style="transform-origin: ${pos.x}px ${pos.y}px;" />`);
            
            // Draw label
            const isSelected = this.selectedCommit === row.commit;
            htmlLabels.push(html`
                <div class="node-label ${isSelected ? 'selected' : ''}" style="left: ${pos.x}px; top: ${pos.y + 15 * this.zoom}px; opacity: ${opacity}; transform: translateX(-50%) scale(${Math.max(0.7, this.zoom)}); transform-origin: top center;">
                    <div class="node-msg" @click=${() => this.selectedCommit = row.commit}>
                        ${row.commit.subject.substring(0, 25)}${row.commit.subject.length > 25 ? '...' : ''}
                    </div>
                    ${row.commit.refs ? row.commit.refs.split(',').map(r => html`<span class="ref-tag">${r.trim()}</span>`) : ''}
                </div>
            `);
        });

        return html`
            <div class="header" @mousedown=${this.startDrag}>
                <div class="title">
                    <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M4 2a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 2Zm10 12a2 2 0 1 1-1.85-2.75l-1.01-.505a.75.75 0 0 1 .673-1.343l1.01.505A2 2 0 0 1 14 14ZM4 10a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 10Zm5-5a2 2 0 1 1-1.85 2.75l-3.02 1.51a.75.75 0 0 1-.673-1.343l3.02-1.51A2 2 0 0 1 9 5Z"/></svg>
                    Git Topology Map
                </div>
                <div class="zoom-controls">
                    <button class="zoom-btn" @click=${() => this.zoom = Math.max(0.5, this.zoom - 0.2)}>-</button>
                    <button class="zoom-btn" @click=${() => this.zoom = Math.min(3.0, this.zoom + 0.2)}>+</button>
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
                ${this.errorMsg ? html`
                    <div style="padding: 16px; color: #f7768e; white-space: pre-wrap; font-family: monospace; overflow-y: auto; flex: 1;">
                        ${this.errorMsg}
                    </div>
                ` : html`
                    <div class="canvas-scroll-view" @wheel=${this.handleWheel}>
                        <div class="canvas-container" style="width: ${svgWidth}px; height: ${svgHeight}px;">
                            <svg width="${svgWidth}" height="${svgHeight}" style="position: absolute; top: 0; left: 0;">
                                ${edgeLines}
                                ${nodeElements}
                            </svg>
                            ${htmlLabels}
                        </div>
                    </div>
                `}
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
