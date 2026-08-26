import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { repeat } from 'lit/directives/repeat.js';
import { globalHoverStyle } from '../../shared-styles';
import { registry } from '../../core/Registry';
import { unsafeHTML } from 'lit/directives/unsafe-html.js';
import { 
    icon_check, 
    icon_add, 
    icon_remove,
    icon_chevron_down,
    icon_chevron_right
} from '../codicons';

interface GitStatusItem {
    status: string;
    path: string;
}

@customElement('vrutti-scm')
export class VruttiScm extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background-color: var(--vrutti-surface);
            color: var(--vrutti-text);
            font-family: var(--vrutti-font, 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif);
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 12px;
            text-transform: uppercase;
            font-size: 11px;
            font-weight: 600;
            letter-spacing: 0.5px;
            color: var(--vrutti-text);
            opacity: 0.7;
        }

        .actions {
            display: flex;
            gap: 4px;
        }

        .icon-btn {
            display: flex;
            align-items: center;
            justify-content: center;
            width: 24px;
            height: 24px;
            border-radius: 4px;
            cursor: pointer;
            color: var(--vrutti-text);
        }

        .icon-btn:hover {
            background-color: var(--vrutti-surface-border);
            color: var(--vrutti-text);
            opacity: 1;
        }

        .commit-box {
            padding: 8px;
            display: flex;
            flex-direction: column;
            gap: 8px;
            border-bottom: 1px solid var(--vrutti-surface-border);
        }

        textarea {
            width: 100%;
            min-height: 60px;
            background: var(--vrutti-bg);
            border: 1px solid var(--vrutti-surface-border);
            color: var(--vrutti-text);
            border-radius: 2px;
            padding: 6px;
            font-family: inherit;
            resize: vertical;
            box-sizing: border-box;
        }

        textarea:focus {
            outline: 1px solid var(--vrutti-accent);
            border-color: transparent;
        }

        button.primary {
            background-color: var(--vrutti-accent);
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 2px;
            cursor: pointer;
            font-weight: 600;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
        }

        button.primary:hover {
            opacity: 0.9;
        }

        button.primary:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }

        .list-section {
            flex-grow: 1;
            overflow-y: auto;
        }

        .section-header {
            display: flex;
            align-items: center;
            padding: 4px 8px;
            cursor: pointer;
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            color: var(--vrutti-text);
        }

        .section-header:hover {
            background-color: var(--vrutti-surface-border);
        }

        .chevron {
            width: 16px;
            height: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-right: 4px;
        }

        .badge {
            background-color: var(--vrutti-surface-border);
            color: var(--vrutti-text);
            border-radius: 10px;
            padding: 0 6px;
            font-size: 10px;
            margin-left: auto;
        }

        .file-item {
            display: flex;
            align-items: center;
            padding: 2px 8px 2px 24px;
            cursor: pointer;
            font-size: 13px;
        }

        .file-item:hover {
            background-color: var(--vrutti-surface-border);
        }

        .file-item .file-actions {
            display: none;
            margin-left: auto;
            gap: 4px;
        }

        .file-item:hover .file-actions {
            display: flex;
        }

        .file-name {
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
            margin-right: 8px;
        }

        .file-status {
            font-family: monospace;
            font-size: 11px;
            margin-right: 8px;
            width: 16px;
            text-align: center;
            font-weight: bold;
        }

        .status-M { color: #e0af68; } /* Modified */
        .status-A { color: #9ece6a; } /* Added */
        .status-D { color: #f7768e; } /* Deleted */
        .status-U { color: #7aa2f7; } /* Untracked */
    `];

    @state() private stagedFiles: GitStatusItem[] = [];
    @state() private unstagedFiles: GitStatusItem[] = [];
    @state() private commitMessage: string = '';
    @state() private stagedExpanded: boolean = true;
    @state() private unstagedExpanded: boolean = true;

    async firstUpdated() {
        await this.refresh();
    }

    private handleRefresh = () => this.refresh();
    private handlePull = () => this.pull();
    private handlePush = () => this.push();
    private handleCommit = () => this.commit();
    private handleStageAll = () => this.stageAll();
    private handleUnstageAll = () => this.unstageAll();
    private handleStageFile = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (detail && detail.path) this.stageFile(detail.path);
    };
    private handleUnstageFile = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (detail && detail.path) this.unstageFile(detail.path);
    };
    private handleViewGraph = () => {
        this.dispatchEvent(new CustomEvent('open-git-graph', { bubbles: true, composed: true }));
    };

    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('scm-refresh', this.handleRefresh);
        window.addEventListener('scm-pull', this.handlePull);
        window.addEventListener('scm-push', this.handlePush);
        window.addEventListener('scm-commit', this.handleCommit);
        window.addEventListener('scm-stage-all', this.handleStageAll);
        window.addEventListener('scm-unstage-all', this.handleUnstageAll);
        window.addEventListener('scm-stage-file', this.handleStageFile);
        window.addEventListener('scm-unstage-file', this.handleUnstageFile);
        window.addEventListener('scm-view-graph', this.handleViewGraph);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('scm-refresh', this.handleRefresh);
        window.removeEventListener('scm-pull', this.handlePull);
        window.removeEventListener('scm-push', this.handlePush);
        window.removeEventListener('scm-commit', this.handleCommit);
        window.removeEventListener('scm-stage-all', this.handleStageAll);
        window.removeEventListener('scm-unstage-all', this.handleUnstageAll);
        window.removeEventListener('scm-stage-file', this.handleStageFile);
        window.removeEventListener('scm-unstage-file', this.handleUnstageFile);
        window.removeEventListener('scm-view-graph', this.handleViewGraph);
    }

    private async runGit(args: string): Promise<{stdout: string, exitCode: number}> {
        const wp = (window as any).currentWorkspace || '';
        if (!wp) return {stdout: '', exitCode: -1};
        try {
            const res = await (window as any).vruttiGitCommand(wp, `git ${args}`);
            return typeof res === 'string' ? JSON.parse(res) : res;
        } catch (e) {
            console.error("Git error:", e);
            return {stdout: '', exitCode: -1};
        }
    }

    public async refresh() {
        const res = await this.runGit('status --porcelain');
        if (res.exitCode !== 0) return;

        const staged: GitStatusItem[] = [];
        const unstaged: GitStatusItem[] = [];

        const lines = res.stdout.split('\n');
        for (const line of lines) {
            if (line.length < 4) continue;
            const x = line[0];
            const y = line[1];
            const path = line.substring(3).trim();

            if (x !== ' ' && x !== '?') {
                staged.push({ status: x, path });
            }
            if (y !== ' ' && x !== '?') {
                unstaged.push({ status: y, path });
            }
            if (x === '?' && y === '?') {
                unstaged.push({ status: 'U', path });
            }
        }

        this.stagedFiles = staged;
        this.unstagedFiles = unstaged;
    }



    private async pull() {
        await this.runGit('pull');
        await this.refresh();
    }

    private async push() {
        await this.runGit('push');
        await this.refresh();
    }

    private async stageFile(path: string) {
        await this.runGit(`add "${path}"`);
        await this.refresh();
    }

    private async unstageFile(path: string) {
        await this.runGit(`restore --staged "${path}"`);
        await this.refresh();
    }

    private async stageAll() {
        await this.runGit('add .');
        await this.refresh();
    }

    private async unstageAll() {
        await this.runGit('restore --staged .');
        await this.refresh();
    }

    private async commit() {
        if (!this.commitMessage.trim()) return;
        const safeMsg = this.commitMessage.replace(/"/g, '\\"');
        const res = await this.runGit(`commit -m "${safeMsg}"`);
        if (res.exitCode === 0) {
            this.commitMessage = '';
            await this.refresh();
        } else {
            console.error("Commit failed:", res.stdout);
        }
    }



    private openFile(path: string) {
        const wp = (window as any).currentWorkspace || '';
        const fullPath = wp + '/' + path;
        const e = new CustomEvent('open-file', {
            detail: { path: fullPath },
            bubbles: true,
            composed: true
        });
        this.dispatchEvent(e);
    }



    private renderStatus(status: string) {
        let cls = 'status-M';
        let text = 'M';
        if (status === 'A') { cls = 'status-A'; text = 'A'; }
        else if (status === 'D') { cls = 'status-D'; text = 'D'; }
        else if (status === 'U' || status === '?') { cls = 'status-U'; text = 'U'; }
        return html`<span class="file-status ${cls}">${text}</span>`;
    }

    render() {
        return html`
            <div class="header">
                <span>Source Control</span>
                <div class="actions">
                    ${(registry.getMenu('scm/title')?.items || []).map(item => html`
                        <div class="icon-btn" title="${item.label}" @click=${() => registry.executeCommand(item.command || '')}>
                            ${item.iconContent ? unsafeHTML(item.iconContent) : ''}
                        </div>
                    `)}
                </div>
            </div>

            <div class="commit-box">
                <textarea 
                    placeholder="Message" 
                    .value=${this.commitMessage}
                    @input=${(e: any) => this.commitMessage = e.target.value}
                ></textarea>
                <button class="primary" ?disabled=${!this.commitMessage.trim() || this.stagedFiles.length === 0} @click=${this.commit}>
                    <span .innerHTML=${icon_check}></span> Commit
                </button>
            </div>

            <div class="list-section">
                <!-- Staged Changes -->
                <div class="section-header" @click=${() => this.stagedExpanded = !this.stagedExpanded}>
                    <div class="chevron" .innerHTML=${this.stagedExpanded ? icon_chevron_down : icon_chevron_right}></div>
                    Staged Changes
                    <div class="badge">${this.stagedFiles.length}</div>
                    <div style="flex-grow: 1;"></div>
                    <div class="icon-btn" title="Unstage All" @click=${(e: Event) => { e.stopPropagation(); this.unstageAll(); }} .innerHTML=${icon_remove}></div>
                </div>
                ${this.stagedExpanded ? repeat(this.stagedFiles, f => f.path, f => html`
                    <div class="file-item" @click=${() => this.openFile(f.path)}>
                        ${this.renderStatus(f.status)}
                        <span class="file-name" title="${f.path}">${f.path}</span>
                        <div class="file-actions">
                            ${(registry.getMenu('scm/resourceState/staged')?.items || []).map(item => html`
                                <div class="icon-btn" title="${item.label}" @click=${(e: Event) => { e.stopPropagation(); registry.executeCommand(item.command || '', { path: f.path }); }}>
                                    ${item.iconContent ? unsafeHTML(item.iconContent) : ''}
                                </div>
                            `)}
                        </div>
                    </div>
                `) : ''}

                <!-- Changes -->
                <div class="section-header" @click=${() => this.unstagedExpanded = !this.unstagedExpanded}>
                    <div class="chevron" .innerHTML=${this.unstagedExpanded ? icon_chevron_down : icon_chevron_right}></div>
                    Changes
                    <div class="badge">${this.unstagedFiles.length}</div>
                    <div style="flex-grow: 1;"></div>
                    <div class="icon-btn" title="Stage All" @click=${(e: Event) => { e.stopPropagation(); this.stageAll(); }} .innerHTML=${icon_add}></div>
                </div>
                ${this.unstagedExpanded ? repeat(this.unstagedFiles, f => f.path, f => html`
                    <div class="file-item" @click=${() => this.openFile(f.path)}>
                        ${this.renderStatus(f.status)}
                        <span class="file-name" title="${f.path}">${f.path}</span>
                        <div class="file-actions">
                            ${(registry.getMenu('scm/resourceState/unstaged')?.items || []).map(item => html`
                                <div class="icon-btn" title="${item.label}" @click=${(e: Event) => { e.stopPropagation(); registry.executeCommand(item.command || '', { path: f.path }); }}>
                                    ${item.iconContent ? unsafeHTML(item.iconContent) : ''}
                                </div>
                            `)}
                        </div>
                    </div>
                `) : ''}
            </div>
        `;
    }
}
