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
  private dynamicTexts: Map<string, string> = new Map();

  @state()
  private dynamicIcons: Map<string, string> = new Map();

  @state()
  private errors = 0;

  @state()
  private warnings = 0;

  async connectedCallback() {
    super.connectedCallback();
    window.addEventListener('editor-cursor-changed', this.handleCursorChange as EventListener);
    window.addEventListener('active-file-changed', this.handleActiveFileChange as EventListener);
    window.addEventListener('vrutti-statusbar-update', this.handleStatusUpdate as EventListener);
    window.addEventListener('setting-changed', this.handleSettingChanged as EventListener);
    registry.addEventListener('change', this.handleRegistryChange);
    this.updateFromRegistry();
    this.fetchGitBranch();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('editor-cursor-changed', this.handleCursorChange as EventListener);
    window.removeEventListener('active-file-changed', this.handleActiveFileChange as EventListener);
    window.removeEventListener('vrutti-statusbar-update', this.handleStatusUpdate as EventListener);
    window.removeEventListener('setting-changed', this.handleSettingChanged as EventListener);
    registry.removeEventListener('change', this.handleRegistryChange);
  }

  private handleSettingChanged = (e: CustomEvent) => {
    if (e.detail && e.detail.key === 'editor.tabSize') {
      this.dynamicTexts.set('indentation', `Spaces: ${e.detail.value}`);
      this.requestUpdate();
    }
  };

  private handleStatusUpdate = (e: CustomEvent) => {
    const detail = e.detail;
    if (detail && detail.id) {
        if (detail.text !== undefined) this.dynamicTexts.set(detail.id, detail.text);
        if (detail.iconContent !== undefined) this.dynamicIcons.set(detail.id, detail.iconContent);
        this.requestUpdate();
    }
  };

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
      const line = e.detail.line || 1;
      const col = e.detail.col || 1;
      this.dynamicTexts.set('cursor-position', `Ln ${line}, Col ${col}`);
      this.requestUpdate();
    }
  };

  private handleActiveFileChange = (e: CustomEvent) => {
    let lang = 'Plain Text';
    if (e.detail && e.detail.path) {
      const ext = e.detail.path.split('.').pop()?.toLowerCase();
      switch (ext) {
        case 'js': lang = 'JavaScript'; break;
        case 'ts': lang = 'TypeScript'; break;
        case 'cpp':
        case 'hpp':
        case 'c':
        case 'h': lang = 'C++'; break;
        case 'json': lang = 'JSON'; break;
        case 'md': lang = 'Markdown'; break;
        case 'html': lang = 'HTML'; break;
        case 'css': lang = 'CSS'; break;
        case 'ps1': lang = 'PowerShell'; break;
        default: lang = 'Plain Text'; break;
      }
    }
    this.dynamicTexts.set('language', lang);
    if (!this.dynamicTexts.has('encoding')) this.dynamicTexts.set('encoding', 'UTF-8');
    if (!this.dynamicTexts.has('eol')) this.dynamicTexts.set('eol', 'CRLF');
    if (!this.dynamicTexts.has('indentation')) this.dynamicTexts.set('indentation', 'Spaces: 4');
    this.requestUpdate();
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
            this.dynamicTexts.set('git-branch', parsed.output.trim());
            this.requestUpdate();
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
    
    let content = this.dynamicTexts.has(item.id) ? this.dynamicTexts.get(item.id) : item.text;
    let icon = this.dynamicIcons.has(item.id) ? this.dynamicIcons.get(item.id) : item.iconContent;

    return html`
      <div class="item" title="${item.tooltip || ''}">
        ${icon ? unsafeSVG(icon) : ''}
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
