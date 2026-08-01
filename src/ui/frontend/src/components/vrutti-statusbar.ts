import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_git_branch, icon_error, icon_warning } from './codicons';

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-statusbar')
export class VruttiStatusBar extends LitElement {

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
    this.fetchGitBranch();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('editor-cursor-changed', this.handleCursorChange as EventListener);
    window.removeEventListener('active-file-changed', this.handleActiveFileChange as EventListener);
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

  render() {
    return html`
      <div class="section left">
        <div class="item" title="Git Branch">
          ${unsafeSVG(icon_git_branch)}
          <span>${this.branch}</span>
        </div>
        <div class="item errors-warnings" title="0 Errors, 0 Warnings">
          ${unsafeSVG(icon_error)} <span>${this.errors}</span>
          ${unsafeSVG(icon_warning)} <span>${this.warnings}</span>
        </div>
      </div>
      <div class="section right">
        <div class="item" title="Go to Line/Column">
          Ln ${this.line}, Col ${this.col}
        </div>
        <div class="item" title="Select Indentation">
          ${this.indent}
        </div>
        <div class="item" title="Select Encoding">
          ${this.encoding}
        </div>
        <div class="item" title="Select End of Line Sequence">
          ${this.eol}
        </div>
        <div class="item" title="Select Language Mode">
          ${this.language}
        </div>
      </div>
    `;
  }
}
