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

    svg {
      width: 24px;
      height: 24px;
      fill: currentColor;
    }

    .pane-action-btn svg {
      width: 16px;
      height: 16px;
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
      <div class="activity-bar">
        <div class="top-icons">
          <!-- Files Icon -->
          <button class="icon-button ${this.activeTab === 'explorer' ? 'active' : ''}" @click="${() => this.selectTab('explorer')}" title="Explorer">
            <svg viewBox="0 0 24 24">
              <path d="M13 9h5.5L13 3.5V9M6 2h8l6 6v12a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4c0-1.11.89-2 2-2m0 2v16h12V10h-6V4H6z" />
            </svg>
          </button>
          <!-- Search Icon -->
          <button class="icon-button ${this.activeTab === 'search' ? 'active' : ''}" @click="${() => this.selectTab('search')}" title="Search">
            <svg viewBox="0 0 24 24">
              <path d="M9.5,3A6.5,6.5 0 0,1 16,9.5C16,11.11 15.41,12.59 14.44,13.73L14.71,14H15.5L20.5,19L19,20.5L14,15.5V14.71L13.73,14.44C12.59,15.41 11.11,16 9.5,16A6.5,6.5 0 0,1 3,9.5A6.5,6.5 0 0,1 9.5,3M9.5,5C7,5 5,7 5,9.5C5,12 7,14 9.5,14C12,14 14,12 14,9.5C14,7 12,5 9.5,5Z" />
            </svg>
          </button>
          <!-- Source Control Icon -->
          <button class="icon-button ${this.activeTab === 'scm' ? 'active' : ''}" @click="${() => this.selectTab('scm')}" title="Source Control">
            <svg viewBox="0 0 24 24">
              <path d="M15,3C12.79,3 11,4.79 11,7C11,7.27 11.05,7.5 11.1,7.74L8.14,11.1C7.81,11.03 7.42,11 7,11C4.79,11 3,12.79 3,15C3,17.21 4.79,19 7,19C9.21,19 11,17.21 11,15C11,14.73 10.95,14.5 10.9,14.26L13.86,10.9C14.19,10.97 14.58,11 15,11C17.21,11 19,9.21 19,7C19,4.79 17.21,3 15,3M15,5A2,2 0 0,1 17,7A2,2 0 0,1 15,9A2,2 0 0,1 13,7A2,2 0 0,1 15,5M7,13A2,2 0 0,1 9,15A2,2 0 0,1 7,17A2,2 0 0,1 5,15A2,2 0 0,1 7,13Z" />
            </svg>
          </button>
          <!-- Debug Icon -->
          <button class="icon-button ${this.activeTab === 'debug' ? 'active' : ''}" @click="${() => this.selectTab('debug')}" title="Run and Debug">
            <svg viewBox="0 0 24 24">
              <path d="M8,5.14V19.14L19,12.14L8,5.14Z" />
            </svg>
          </button>
          <!-- Extensions Icon -->
          <button class="icon-button ${this.activeTab === 'extensions' ? 'active' : ''}" @click="${() => this.selectTab('extensions')}" title="Extensions">
            <svg viewBox="0 0 24 24">
              <path d="M19,3H5C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V5C21,3.89 20.1,3 19,3M19,19H5V5H19V19M17,17H11V11H17V17M13,15H15V13H13V15M17,9H7V11H9V17H7V9H17Z" />
            </svg>
          </button>
        </div>
        <div class="bottom-icons">
          <!-- Chevron Toggle -->
          <button class="icon-button" @click="${this.toggleSidebar}" title="Toggle Sidebar">
            <svg viewBox="0 0 24 24">
              ${this.isOpen 
                ? html`<path d="M15.41,16.58L10.83,12L15.41,7.41L14,6L8,12L14,18L15.41,16.58Z" />` 
                : html`<path d="M8.59,16.58L13.17,12L8.59,7.41L10,6L16,12L10,18L8.59,16.58Z" />`
              }
            </svg>
          </button>
        </div>
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'}">
        <div class="pane-header">
          <span>${this.activeTab}</span>
          <button class="pane-action-btn" @click="${() => this.isOpen = false}" title="Minimize">
            <svg viewBox="0 0 24 24"><path d="M19 13H5v-2h14v2z"/></svg>
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
