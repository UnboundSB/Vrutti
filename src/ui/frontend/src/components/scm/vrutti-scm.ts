import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { SCMModel, SCMFile } from './scmModel';
import { icon_chevron_down, icon_chevron_right } from '../codicons';

// Basic SVG icons for SCM actions
const icon_add = '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor"><path d="M14 7v1H8v6H7V8H1V7h6V1h1v6h6z"/></svg>';
const icon_remove = '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor"><path d="M14 7v1H2V7h12z"/></svg>';
const icon_check = '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor"><path fill-rule="evenodd" clip-rule="evenodd" d="M14.431 3.323l-8.47 10-.79-.036-3.35-4.77.818-.574 2.978 4.24 8.051-9.506.763.646z"/></svg>';
const icon_sync = '<svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor"><path fill-rule="evenodd" clip-rule="evenodd" d="M14.5 4.5l-2.5 2.5h-1l.7-.7H2.5v-1h9.2l-.7-.7v-1l2.5 2.5v-.6zm-13 7l2.5-2.5h1l-.7.7h9.2v1H4.3l.7.7v1L2.5 11.5v.6z"/></svg>';

@customElement('vrutti-scm')
export class VruttiSCM extends LitElement {
  @state() private model = new SCMModel();
  @state() private commitMessage = '';
  @state() private stagedExpanded = true;
  @state() private changesExpanded = true;

  async connectedCallback() {
    super.connectedCallback();
    await this.refreshStatus();
  }

  private get workspacePath() {
    return (window as any).currentWorkspace || '';
  }

  private async runGitCommand(cmd: string): Promise<{ stdout: string, exitCode: number }> {
    return new Promise((resolve) => {
      if ((window as any).vruttiGitCommand) {
        // Evaluate native binding sync via IPC is not supported directly in the stub,
        // wait, the binding in Window.cpp returns JSON string synchronously via eval.
        // Actually webview binds are sync or async depending on the wrapper, but usually they return a promise if called from JS.
        const res = (window as any).vruttiGitCommand(this.workspacePath, cmd);
        if (res && typeof res.then === 'function') {
           res.then((jsonStr: string) => resolve(JSON.parse(jsonStr)));
        } else {
           resolve(JSON.parse(res));
        }
      } else {
        resolve({ stdout: '', exitCode: -1 });
      }
    });
  }

  private async refreshStatus() {
    if (!this.workspacePath) return;
    const res = await this.runGitCommand('git status --porcelain');
    if (res.exitCode === 0 || res.exitCode === 1) { // git status might exit 1 if changes? No, it exits 0 unless not a repo.
      this.model.parseGitStatus(res.stdout);
      this.requestUpdate();
    } else {
      // Maybe not a git repo, could init?
      this.model.parseGitStatus('');
      this.requestUpdate();
    }
  }

  private async stageFile(file: SCMFile) {
    await this.runGitCommand(`git add "${file.resource}"`);
    await this.refreshStatus();
  }

  private async unstageFile(file: SCMFile) {
    await this.runGitCommand(`git reset HEAD "${file.resource}"`);
    await this.refreshStatus();
  }

  private async commit() {
    if (!this.commitMessage) return;
    const msg = this.commitMessage.replace(/"/g, '\\"');
    await this.runGitCommand(`git commit -m "${msg}"`);
    this.commitMessage = '';
    await this.refreshStatus();
  }

  private async sync() {
    // Basic pull then push
    await this.runGitCommand('git pull');
    await this.runGitCommand('git push');
    await this.refreshStatus();
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100%;
      color: var(--vrutti-text, #636b95);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      font-size: 13px;
    }
    
    .scm-header {
      padding: 10px;
      display: flex;
      flex-direction: column;
      gap: 10px;
    }

    .input-box {
      width: 100%;
      box-sizing: border-box;
      background: var(--vrutti-bg, #0f111a);
      border: 1px solid var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #a6accd);
      padding: 8px;
      border-radius: 2px;
      resize: vertical;
      min-height: 60px;
      font-family: inherit;
    }

    .input-box:focus {
      outline: 1px solid var(--vrutti-accent, #82aaff);
      border-color: transparent;
    }

    .button-row {
      display: flex;
      gap: 5px;
    }

    .btn {
      flex: 1;
      background: var(--vrutti-accent, #82aaff);
      color: #000;
      border: none;
      padding: 6px;
      border-radius: 2px;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 5px;
      font-weight: 600;
    }
    
    .btn:hover {
      background: var(--vrutti-accent-hover, #a6c4ff);
    }

    .btn-secondary {
      flex: 0 0 auto;
      background: transparent;
      color: var(--vrutti-text, #636b95);
      border: 1px solid var(--vrutti-surface-border, #23273b);
    }
    
    .btn-secondary:hover {
      background: var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #a6accd);
    }

    .scm-list {
      flex: 1;
      overflow-y: auto;
    }

    .section-header {
      display: flex;
      align-items: center;
      padding: 4px 8px;
      cursor: pointer;
      font-size: 11px;
      font-weight: 700;
      text-transform: uppercase;
    }

    .section-header:hover {
      background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.05));
    }
    
    .chevron {
      width: 16px;
      height: 16px;
      margin-right: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    
    .badge {
      background: var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #a6accd);
      border-radius: 10px;
      padding: 1px 6px;
      font-size: 10px;
      margin-left: auto;
    }

    .file-item {
      display: flex;
      align-items: center;
      padding: 4px 8px 4px 24px;
      cursor: pointer;
    }
    
    .file-item:hover {
      background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.05));
    }

    .file-name {
      color: var(--vrutti-text-bright, #a6accd);
      margin-right: 6px;
    }

    .file-dir {
      color: var(--vrutti-text, #636b95);
      font-size: 11px;
      opacity: 0.7;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      flex: 1;
    }
    
    .file-status {
      font-size: 11px;
      margin: 0 8px;
      font-weight: 600;
    }
    
    .status-modified { color: #e2c08d; }
    .status-added { color: #81b88b; }
    .status-deleted { color: #f14c4c; }
    .status-untracked { color: #81b88b; }

    .file-actions {
      display: none;
      align-items: center;
    }

    .file-item:hover .file-actions {
      display: flex;
    }

    .action-icon {
      width: 20px;
      height: 20px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      opacity: 0.7;
    }
    
    .action-icon:hover {
      opacity: 1;
      background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.1));
      border-radius: 3px;
    }
    
    svg {
      width: 16px;
      height: 16px;
    }
  `;

  render() {
    const changesCount = this.model.unstagedFiles.length + this.model.untrackedFiles.length;
    const stagedCount = this.model.stagedFiles.length;

    return html`
      <div class="scm-header">
        <textarea 
          class="input-box" 
          placeholder="Message (Ctrl+Enter to commit)"
          .value="${this.commitMessage}"
          @input="${(e: any) => this.commitMessage = e.target.value}"
          @keydown="${(e: KeyboardEvent) => {
            if (e.ctrlKey && e.key === 'Enter') this.commit();
          }}"
        ></textarea>
        <div class="button-row">
          <button class="btn" @click="${this.commit}" ?disabled="${!this.commitMessage}">
            ${unsafeSVG(icon_check)} Commit
          </button>
          <button class="btn btn-secondary" @click="${this.sync}" title="Sync Changes">
            ${unsafeSVG(icon_sync)}
          </button>
        </div>
      </div>
      
      <div class="scm-list">
        ${stagedCount > 0 ? html`
          <div class="section-header" @click="${() => this.stagedExpanded = !this.stagedExpanded}">
            <div class="chevron">${unsafeSVG(this.stagedExpanded ? icon_chevron_down : icon_chevron_right)}</div>
            <span>Staged Changes</span>
            <span class="badge">${stagedCount}</span>
          </div>
          ${this.stagedExpanded ? this.model.stagedFiles.map(f => this.renderFile(f, true)) : ''}
        ` : ''}

        ${changesCount > 0 ? html`
          <div class="section-header" @click="${() => this.changesExpanded = !this.changesExpanded}">
            <div class="chevron">${unsafeSVG(this.changesExpanded ? icon_chevron_down : icon_chevron_right)}</div>
            <span>Changes</span>
            <span class="badge">${changesCount}</span>
          </div>
          ${this.changesExpanded ? [
            ...this.model.unstagedFiles.map(f => this.renderFile(f, false)),
            ...this.model.untrackedFiles.map(f => this.renderFile(f, false))
          ] : ''}
        ` : ''}
      </div>
    `;
  }

  private renderFile(file: SCMFile, isStaged: boolean) {
    const statusLetter = file.status === 'modified' ? 'M' : file.status === 'added' || file.status === 'untracked' ? 'U' : file.status === 'deleted' ? 'D' : 'M';
    
    return html`
      <div class="file-item" title="${file.resource}">
        <span class="file-name">${file.name}</span>
        <span class="file-dir">${file.directory}</span>
        
        <div class="file-actions">
          ${isStaged 
            ? html`<div class="action-icon" @click="${(e: Event) => { e.stopPropagation(); this.unstageFile(file); }}" title="Unstage Changes">${unsafeSVG(icon_remove)}</div>`
            : html`<div class="action-icon" @click="${(e: Event) => { e.stopPropagation(); this.stageFile(file); }}" title="Stage Changes">${unsafeSVG(icon_add)}</div>`
          }
        </div>
        
        <span class="file-status status-${file.status}">${statusLetter}</span>
      </div>
    `;
  }
}
