import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_files, icon_search, icon_source_control, icon_debug_alt, icon_extensions, icon_chevron_left, icon_chevron_right, icon_close } from './codicons';
import './explorer/vrutti-explorer';
import { ExplorerModel, ExplorerItem } from './explorer/explorerModel';

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-sidebar')
export class VruttiSidebar extends LitElement {
  @state()
  private isDockOpen = false;

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

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      position: absolute;
      left: 0;
      top: 0;
      bottom: 0;
      height: 100%;
      z-index: 50;
      transform: translateX(-100%);
      transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      background-color: var(--vrutti-surface, #13151f);
      border-right: 1px solid var(--vrutti-surface-border, #23273b);
      box-shadow: 4px 0 15px rgba(0,0,0,0.4);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
    }
    
    :host(.dock-open) {
      transform: translateX(0);
    }

    .dock-toggle {
      position: absolute;
      right: -24px;
      top: 50%;
      transform: translateY(-50%);
      width: 24px;
      height: 48px;
      background: var(--vrutti-surface, #13151f);
      border: 1px solid var(--vrutti-surface-border, #23273b);
      border-left: none;
      border-radius: 0 8px 8px 0;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--vrutti-text, #636b95);
      cursor: pointer;
      box-shadow: 4px 0 10px rgba(0,0,0,0.2);
      z-index: 60;
    }

    .dock-toggle:hover {
      color: var(--vrutti-text-bright, #a6accd);
      background: var(--vrutti-surface-border, #23273b);
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
      overflow-y: auto;
    }

    .activity-bar::-webkit-scrollbar {
      display: none;
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
      overflow-x: hidden;
    }

    svg {
      width: 24px;
      height: 24px;
    }

    .pane-action-btn svg {
      width: 14px;
      height: 14px;
    }
  `];

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

  toggleDock() {
      this.isDockOpen = !this.isDockOpen;
      if (this.isDockOpen) {
        this.classList.add('dock-open');
      } else {
        this.classList.remove('dock-open');
      }
    }

    render() {
      return html`
        <div class="dock-toggle" @click="${this.toggleDock}" title="Toggle Sidebar">
          ${unsafeSVG(this.isDockOpen ? icon_chevron_left : icon_chevron_right)}
        </div>
        <div class="activity-bar">
        <div class="top-icons">
          <!-- Files Icon -->
          <button class="icon-button ${this.activeTab === 'explorer' ? 'active' : ''}" @click="${() => this.selectTab('explorer')}" title="Explorer">
            ${unsafeSVG(icon_files)}
          </button>
          <!-- Search Icon -->
          <button class="icon-button ${this.activeTab === 'search' ? 'active' : ''}" @click="${() => this.selectTab('search')}" title="Search">
            ${unsafeSVG(icon_search)}
          </button>
          <!-- Source Control Icon -->
          <button class="icon-button ${this.activeTab === 'scm' ? 'active' : ''}" @click="${() => this.selectTab('scm')}" title="Source Control">
            ${unsafeSVG(icon_source_control)}
          </button>
          <!-- Debug Icon -->
          <button class="icon-button ${this.activeTab === 'debug' ? 'active' : ''}" @click="${() => this.selectTab('debug')}" title="Run and Debug">
            ${unsafeSVG(icon_debug_alt)}
          </button>
          <!-- Extensions Icon -->
          <button class="icon-button ${this.activeTab === 'extensions' ? 'active' : ''}" @click="${() => this.selectTab('extensions')}" title="Extensions">
            ${unsafeSVG(icon_extensions)}
          </button>
        </div>
        <div class="bottom-icons">
        </div>
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'}">
        <div class="pane-header">
          <span>${this.activeTab}</span>
          <button class="pane-action-btn" @click="${() => this.isOpen = false}" title="Minimize">
            ${unsafeSVG(icon_close)}
          </button>
        </div>
        <div class="pane-content" style="padding: 0; overflow-y: auto; overflow-x: hidden;">
          ${this.activeTab === 'explorer' 
            ? html`<vrutti-explorer .item="${this.explorerRoot}"></vrutti-explorer>` 
            : html`<div style="padding: 15px; opacity: 0.5;">${this.activeTab} panel not yet implemented.</div>`
          }
        </div>
      </div>
    `;
  }
}
