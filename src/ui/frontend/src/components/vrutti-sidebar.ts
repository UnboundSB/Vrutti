import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './explorer/vrutti-explorer';
import { ExplorerModel, ExplorerItem } from './explorer/explorerModel';

@customElement('vrutti-sidebar')
export class VruttiSidebar extends LitElement {
  @state()
  private isOpen = true;

  @state()
  private activeTab = 'explorer';

  @state()
  private explorerRoot!: ExplorerItem;

  connectedCallback() {
    super.connectedCallback();
    const model = new ExplorerModel();
    model.setRoot({
      name: 'Vrutti IDE Workspace',
      isDirectory: true,
      resource: 'file:///d:/vrutti/vrutti_ide',
      children: [
        { name: 'src', isDirectory: true, resource: 'file:///d:/vrutti/vrutti_ide/src', children: [
          { name: 'main.ts', isDirectory: false, resource: 'file:///d:/vrutti/vrutti_ide/src/main.ts' },
          { name: 'ThemeBridge.ts', isDirectory: false, resource: 'file:///d:/vrutti/vrutti_ide/src/ThemeBridge.ts' }
        ]},
        { name: 'package.json', isDirectory: false, resource: 'file:///d:/vrutti/vrutti_ide/package.json' },
        { name: 'README.md', isDirectory: false, resource: 'file:///d:/vrutti/vrutti_ide/README.md' }
      ]
    });
    this.explorerRoot = model.root!;
  }

  static styles = css`
    :host {
      display: flex;
      height: 100%;
      background-color: var(--vrutti-surface, #13151f);
      border-right: 1px solid var(--vrutti-surface-border, #23273b);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
    }

    .activity-bar {
      width: 48px;
      height: 100%;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: space-between;
      padding-top: 10px;
      padding-bottom: 10px;
      background-color: var(--vrutti-bg, #0f111a);
      border-right: 1px solid var(--vrutti-surface-border, #23273b);
      z-index: 10;
    }

    .top-icons, .bottom-icons {
      display: flex;
      flex-direction: column;
      align-items: center;
      width: 100%;
    }

    .icon-button {
      background: none;
      border: none;
      color: var(--vrutti-text, #636b95);
      width: 48px;
      height: 48px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      transition: color 0.2s;
      position: relative;
    }

    .icon-button:hover {
      color: var(--vrutti-text-bright, #a6accd);
    }

    .icon-button.active {
      color: var(--vrutti-accent, #82aaff);
    }

    .icon-button.active::before {
      content: '';
      position: absolute;
      left: 0;
      top: 0;
      bottom: 0;
      width: 2px;
      background-color: var(--vrutti-accent, #82aaff);
    }

    .sidebar-pane {
      width: 250px;
      height: 100%;
      overflow: hidden;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      display: flex;
      flex-direction: column;
    }

    .sidebar-pane.collapsed {
      width: 0px;
      border-right: none;
    }

    .pane-header {
      padding: 10px 16px;
      font-size: 11px;
      text-transform: uppercase;
      font-weight: 600;
      color: var(--vrutti-text-bright, #a6accd);
      letter-spacing: 0.5px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      white-space: nowrap;
    }

    .pane-action-btn {
      background: none;
      border: none;
      color: var(--vrutti-text, #636b95);
      cursor: pointer;
      width: 20px;
      height: 20px;
      display: flex;
      align-items: center;
      justify-content: center;
      border-radius: 4px;
      padding: 0;
    }

    .pane-action-btn:hover {
      background: var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #fff);
    }

    .pane-content {
      padding: 0 10px;
      color: var(--vrutti-text, #636b95);
      font-size: 13px;
      white-space: nowrap;
    }

    .pane-action-btn i {
      font-size: 14px;
    }
  `;

  selectTab(tab: string) {
    if (this.activeTab === tab) {
      this.isOpen = !this.isOpen;
    } else {
      this.activeTab = tab;
      this.isOpen = true;
    }
  }

  toggleSidebar() {
    this.isOpen = !this.isOpen;
  }

  render() {
    return html`
      <link href="https://cdn.jsdelivr.net/npm/@vscode/codicons/dist/codicon.css" rel="stylesheet" />
      <style>
        .codicon {
          font-size: 24px;
        }
      </style>
      <div class="activity-bar">
        <div class="top-icons">
          <!-- Files Icon -->
          <button class="icon-button ${this.activeTab === 'explorer' ? 'active' : ''}" @click="${() => this.selectTab('explorer')}" title="Explorer">
            <i class="codicon codicon-files"></i>
          </button>
          <!-- Search Icon -->
          <button class="icon-button ${this.activeTab === 'search' ? 'active' : ''}" @click="${() => this.selectTab('search')}" title="Search">
            <i class="codicon codicon-search"></i>
          </button>
          <!-- Source Control Icon -->
          <button class="icon-button ${this.activeTab === 'scm' ? 'active' : ''}" @click="${() => this.selectTab('scm')}" title="Source Control">
            <i class="codicon codicon-source-control"></i>
          </button>
          <!-- Debug Icon -->
          <button class="icon-button ${this.activeTab === 'debug' ? 'active' : ''}" @click="${() => this.selectTab('debug')}" title="Run and Debug">
            <i class="codicon codicon-debug-alt"></i>
          </button>
          <!-- Extensions Icon -->
          <button class="icon-button ${this.activeTab === 'extensions' ? 'active' : ''}" @click="${() => this.selectTab('extensions')}" title="Extensions">
            <i class="codicon codicon-extensions"></i>
          </button>
        </div>
        <div class="bottom-icons">
          <!-- Chevron Toggle -->
          <button class="icon-button" @click="${this.toggleSidebar}" title="Toggle Sidebar">
            <i class="codicon ${this.isOpen ? 'codicon-chevron-left' : 'codicon-chevron-right'}"></i>
          </button>
        </div>
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'}">
        <div class="pane-header">
          <span>${this.activeTab}</span>
          <button class="pane-action-btn" @click="${() => this.isOpen = false}" title="Minimize">
            <i class="codicon codicon-close"></i>
          </button>
        </div>
        <div class="pane-content" style="padding: 0; overflow-y: auto;">
          ${this.activeTab === 'explorer' 
            ? html`<vrutti-explorer .item="${this.explorerRoot}"></vrutti-explorer>` 
            : html`<div style="padding: 15px; opacity: 0.5;">${this.activeTab} panel not yet implemented.</div>`
          }
        </div>
      </div>
    `;
  }
}
