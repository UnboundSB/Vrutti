import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_error, icon_warning } from './codicons';
import { registry, StatusBarContribution } from '../core/Registry';

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-statusbar')
export class VruttiStatusBar extends LitElement {

  @state()
  private leftItems: StatusBarContribution[] = [];

  @state()
  private rightItems: StatusBarContribution[] = [];

  @state()
  private branch = 'main';

  @state()
  private errors = 0;

  @state()
  private warnings = 0;

  @state()
  private line = 1;

  @state()
  private col = 1;

  @state()
  private indent = 'Spaces: 4';

  @state()
  private encoding = 'UTF-8';

  @state()
  private eol = 'CRLF';

  @state()
  private language = 'TypeScript';

  async connectedCallback() {
    super.connectedCallback();
    window.addEventListener('editor-cursor-changed', this.handleCursorChange as EventListener);
    window.addEventListener('active-file-changed', this.handleActiveFileChange as EventListener);
    registry.addEventListener('change', this.handleRegistryChange);
    this.updateFromRegistry();
    this.fetchGitBranch();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('editor-cursor-changed', this.handleCursorChange as EventListener);
    window.removeEventListener('active-file-changed', this.handleActiveFileChange as EventListener);
    registry.removeEventListener('change', this.handleRegistryChange);
  }

  private handleRegistryChange = (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && detail.type === 'statusbar') {
      this.updateFromRegistry();
    }
  };

  private updateFromRegistry() {
    this.leftItems = registry.getStatusBarItems('left');
    this.rightItems = registry.getStatusBarItems('right');
  }

  private handleCursorChange = (e: CustomEvent) => {
    if (e.detail) {
      this.line = e.detail.line || 1;
      this.col = e.detail.col || 1;
    }
  };

  private handleActiveFileChange = (e: CustomEvent) => {
    if (e.detail && e.detail.path) {
      const ext = e.detail.path.split('.').pop()?.toLowerCase();
      switch (ext) {
        case 'js': this.language = 'JavaScript'; break;
        case 'ts': this.language = 'TypeScript'; break;
        case 'cpp':
        case 'hpp':
        case 'c':
        case 'h': this.language = 'C++'; break;
        case 'json': this.language = 'JSON'; break;
        case 'md': this.language = 'Markdown'; break;
        case 'html': this.language = 'HTML'; break;
        case 'css': this.language = 'CSS'; break;
        case 'ps1': this.language = 'PowerShell'; break;
        default: this.language = 'Plain Text'; break;
      }
    } else {
      this.language = 'Plain Text';
    }
  };

  private async fetchGitBranch() {
    try {
      if ((window as any).vruttiGitCommand) {
        let actualDir = (window as any).currentWorkspace || '.';
        if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
        else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);

        const res = await (window as any).vruttiGitCommand(actualDir, "git rev-parse --abbrev-ref HEAD");
        const parsed = JSON.parse(res);
        if (parsed.success && parsed.output) {
            this.branch = parsed.output.trim();
        }
      }
    } catch (e) {
      console.error("Failed to fetch git branch", e);
    }
  }

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      height: 22px;
      width: 100%;
      background-color: var(--vrutti-bg, #0f111a);
      border-top: 1px solid var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text, #636b95);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      font-size: 11px;
      user-select: none;
      align-items: center;
      justify-content: space-between;
      box-sizing: border-box;
      padding: 0 8px;
    }

    .section {
      display: flex;
      align-items: center;
      height: 100%;
    }

    .item {
      display: flex;
      align-items: center;
      height: 100%;
      padding: 0 6px;
      cursor: pointer;
      transition: background-color 0.1s ease, color 0.1s ease;
    }

    .item:hover {
      color: var(--vrutti-text-bright, #fff);
    }

    .item svg {
      width: 14px;
      height: 14px;
      margin-right: 4px;
    }

    .errors-warnings {
      display: flex;
      align-items: center;
    }

    .errors-warnings span {
      margin-right: 6px;
    }
  `];

  private renderItem(item: StatusBarContribution) {
    if (item.component === 'errors-warnings') {
      return html`
        <div class="item errors-warnings" title="0 Errors, 0 Warnings">
          ${unsafeSVG(icon_error)} <span>${this.errors}</span>
          ${unsafeSVG(icon_warning)} <span>${this.warnings}</span>
        </div>
      `;
    }
    
    let content = item.text || '';
    if (item.id === 'git-branch') content = this.branch;
    if (item.id === 'cursor-position') content = `Ln ${this.line}, Col ${this.col}`;
    if (item.id === 'indentation') content = this.indent;
    if (item.id === 'encoding') content = this.encoding;
    if (item.id === 'eol') content = this.eol;
    if (item.id === 'language') content = this.language;

    return html`
      <div class="item" title="${item.tooltip || ''}">
        ${item.iconContent ? unsafeSVG(item.iconContent) : ''}
        ${content ? html`<span>${content}</span>` : ''}
      </div>
    `;
  }

  render() {
    return html`
      <div class="section left">
        ${this.leftItems.map(item => this.renderItem(item))}
      </div>
      <div class="section right">
        ${this.rightItems.map(item => this.renderItem(item))}
      </div>
    `;
  }
}
