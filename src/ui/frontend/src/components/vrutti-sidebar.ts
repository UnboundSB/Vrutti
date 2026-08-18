import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_files, icon_search, icon_source_control, icon_debug_alt, icon_extensions, icon_close, icon_chevron_left, icon_chevron_right } from './codicons';
import './explorer/vrutti-explorer';
import './vrutti-search';
import './scm/vrutti-scm';
import './vrutti-extensions';
import './vrutti-debug-sidebar';
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
  private explorerRoot?: ExplorerItem;

  @state()
  private sidebarWidth = 250;

  @state()
  private isResizing = false;

  private startResize(e: MouseEvent) {
    e.preventDefault();
    this.isResizing = true;
    document.addEventListener('mousemove', this.doResize);
    document.addEventListener('mouseup', this.stopResize);
    document.body.style.cursor = 'col-resize';
  }

  private doResize = (e: MouseEvent) => {
    if (!this.isResizing) return;
    let newWidth = e.clientX - 48; // Activity bar is 48px wide
    if (newWidth < 150) newWidth = 150;
    if (newWidth > 800) newWidth = 800;
    this.sidebarWidth = newWidth;
  };

  private stopResize = () => {
    this.isResizing = false;
    document.removeEventListener('mousemove', this.doResize);
    document.removeEventListener('mouseup', this.stopResize);
    document.body.style.cursor = '';
  };

  async connectedCallback() {
    super.connectedCallback();
    window.addEventListener('workspace-changed', this.handleWorkspaceChanged);
    
    let initialPath = '';
    if ((window as any).vruttiGetInitialWorkspace) {
      try {
        const json = await (window as any).vruttiGetInitialWorkspace();
        if (json && json.path && json.path !== "") {
          initialPath = json.path;
        }
      } catch (e) {
        console.warn("Failed to get initial workspace from backend, falling back to local storage.", e);
      }
    }
    
    window.addEventListener('switch-to-debug-panel', () => {
      this.selectTab('debug');
      this.isOpen = true;
      if (!this.isDockOpen) {
        this.toggleDock();
      }
    });

    // If no initial path was provided via CLI argument, restore from localStorage
    if (!initialPath) {
      const savedPath = localStorage.getItem('lastWorkspace');
      if (savedPath) {
        initialPath = savedPath;
      }
    }
    
    if (initialPath) {
      (window as any).currentWorkspace = initialPath;
      localStorage.setItem('lastWorkspace', initialPath);
      let folderName = initialPath.split(/[\\/]/).pop() || 'Workspace';
      await this.loadWorkspace(initialPath, folderName);
    } else {
      (window as any).currentWorkspace = '';
      this.explorerRoot = undefined;
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('workspace-changed', this.handleWorkspaceChanged);
  }

  private handleWorkspaceChanged = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail.path) {
      (window as any).currentWorkspace = detail.path;
      localStorage.setItem('lastWorkspace', detail.path);
      let folderName = detail.path.split(/[\\/]/).pop() || 'Workspace';
      await this.loadWorkspace(detail.path, folderName);
    }
  };

  private async loadWorkspace(path: string, name: string) {
    const model = new ExplorerModel();
    model.setRoot({
      name: name,
      isDirectory: true,
      resource: path
    });
    this.explorerRoot = model.root!;
    this.explorerRoot.isExpanded = true;
    await this.explorerRoot.loadChildren();
    this.requestUpdate();
  }

  private async createFileAtRoot(e: Event) {
    e.stopPropagation();
    const name = window.prompt("Enter new file name:");
    if (!name || !this.explorerRoot) return;
    
    let basePath = this.explorerRoot.resource;
    if (basePath.startsWith('file:///')) basePath = basePath.substring(8);
    else if (basePath.startsWith('file://')) basePath = basePath.substring(7);
    
    const targetPath = basePath + "/" + name;
    if ((window as any).vruttiCreateFile) {
      await (window as any).vruttiCreateFile(targetPath);
      this.explorerRoot.childrenLoaded = false;
      await this.explorerRoot.loadChildren();
      this.requestUpdate();
    }
  }

  private async createFolderAtRoot(e: Event) {
    e.stopPropagation();
    const name = window.prompt("Enter new folder name:");
    if (!name || !this.explorerRoot) return;
    
    let basePath = this.explorerRoot.resource;
    if (basePath.startsWith('file:///')) basePath = basePath.substring(8);
    else if (basePath.startsWith('file://')) basePath = basePath.substring(7);
    
    const targetPath = basePath + "/" + name;
    if ((window as any).vruttiCreateFolder) {
      await (window as any).vruttiCreateFolder(targetPath);
      this.explorerRoot.childrenLoaded = false;
      await this.explorerRoot.loadChildren();
      this.requestUpdate();
    }
  }

  private async refreshRoot(e: Event) {
    e.stopPropagation();
    if (!this.explorerRoot) return;
    this.explorerRoot.childrenLoaded = false;
    await this.explorerRoot.loadChildren();
    this.requestUpdate();
  }

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      position: relative;
      height: 100%;
      z-index: 50;
      background-color: var(--vrutti-surface, #13151f);
      border-right: 1px solid var(--vrutti-surface-border, #23273b);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
    }
    
    :host(.dock-open) {
      /* No longer using transform */
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
      transition: opacity 0.2s;
    }

    .dock-toggle.collapsed {
      opacity: 0;
      pointer-events: none;
    }

    :host(:hover) .dock-toggle.collapsed {
      opacity: 1;
      pointer-events: auto;
    }

    .dock-toggle:hover {
      color: var(--vrutti-text-bright, #a6accd);
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
      height: 100%;
      overflow: hidden;
      transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      display: flex;
      flex-direction: column;
      position: relative;
    }

    .sidebar-pane.resizing {
      transition: none;
    }

    .sidebar-pane.collapsed {
      border-right: none;
    }

    .sidebar-resizer {
      position: absolute;
      top: 0;
      right: -2px;
      width: 5px;
      height: 100%;
      cursor: col-resize;
      z-index: 100;
    }
    .sidebar-resizer:hover, .sidebar-resizer.active {
      background: var(--vrutti-accent, #82aaff);
      opacity: 0.5;
    }

    .sidebar-resizer:hover, .sidebar-pane.resizing .sidebar-resizer {
      background-color: var(--vrutti-accent, #82aaff);
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
      color: var(--vrutti-text-bright, #fff);
    }

    .pane-content {
      padding: 0 10px;
      color: var(--vrutti-text, #636b95);
      font-size: 13px;
      white-space: nowrap;
      overflow-x: hidden;
      flex: 1;
      min-height: 0;
    }

    svg {
      width: 24px;
      height: 24px;
    }

    .pane-action-btn svg {
      width: 14px;
      height: 14px;
    }

    .workspace-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 4px 8px;
      cursor: pointer;
      user-select: none;
    }

    .workspace-header:hover {
      background-color: var(--vrutti-surface-border, rgba(255, 255, 255, 0.1));
    }

    .workspace-title {
      display: flex;
      align-items: center;
      font-size: 11px;
      font-weight: bold;
    }

    .workspace-actions {
      display: none;
      align-items: center;
    }

    .workspace-header:hover .workspace-actions {
      display: flex;
    }

    .action-icon {
      width: 16px;
      height: 16px;
      margin-left: 6px;
      opacity: 0.6;
      transition: opacity 0.2s;
    }

    .action-icon:hover {
      opacity: 1;
    }

    .chevron {
      width: 16px;
      height: 16px;
      margin-right: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: transform 0.2s ease;
      opacity: 0.8;
    }

    .chevron.expanded {
      transform: rotate(90deg);
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

  @state()
  private isDocked = false;

  toggleSidebar() {
    this.isOpen = !this.isOpen;
  }

  toggleDock() {
      this.isDocked = !this.isDocked;
      if (this.isDocked) {
        this.classList.add('docked');
      } else {
        this.classList.remove('docked');
      }
      this.requestUpdate();
    }

    render() {
      return html`
        <!-- removed dock toggle to simplify -->
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
      <div class="dock-toggle ${this.isOpen ? '' : 'collapsed'}" @click="${() => this.isOpen = !this.isOpen}">
        ${this.isOpen ? unsafeSVG(icon_chevron_left) : unsafeSVG(icon_chevron_right)}
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'} ${this.isResizing ? 'resizing' : ''}" style="width: ${this.isOpen ? this.sidebarWidth : 0}px;">
        <div class="pane-header">
          <span>${this.activeTab}</span>
          <button class="pane-action-btn" @click="${() => this.isOpen = false}" title="Minimize">
            ${unsafeSVG(icon_close)}
          </button>
        </div>
        <div class="pane-content" style="padding: 0; overflow-y: auto; overflow-x: hidden;">
          ${this.activeTab === 'explorer' 
            ? html`
                <div class="workspace-section">
                  <div class="workspace-header" @click="${() => {
                    if (this.explorerRoot) {
                      this.explorerRoot.isExpanded = !this.explorerRoot.isExpanded;
                      this.requestUpdate();
                    }
                  }}">
                    <div class="workspace-title">
                      <div class="chevron ${this.explorerRoot?.isExpanded ? 'expanded' : ''}">
                        <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor"><path d="M8.59 16.59L13.17 12 8.59 7.41 10 6l6 6-6 6-1.41-1.41z"/></svg>
                      </div>
                      <span style="font-weight: 700;">${this.explorerRoot?.name?.toUpperCase() || 'NO FOLDER OPENED'}</span>
                    </div>
                    <div class="workspace-actions" @click="${(e: Event) => e.stopPropagation()}">
                      <svg class="action-icon" title="New File" @click="${this.createFileAtRoot}" viewBox="0 0 16 16" fill="currentColor">
                        <path d="M9 1H4v14h9V6H9V1zm0 1.414L11.586 5H9V2.414zM4 0h6l5 5v10H4V0z"/><path d="M5.5 8.5h4v1h-4zm0 2h4v1h-4zm0 2h4v1h-4z"/>
                      </svg>
                      <svg class="action-icon" title="New Folder" @click="${this.createFolderAtRoot}" viewBox="0 0 16 16" fill="currentColor">
                        <path d="M14 4.5V14H2V3h3.5l1.5 1.5H14zm-1 8.5V5.5H7.5L6 4H3v9h10z"/><path d="M7.5 7h1v2h2v1h-2v2h-1v-2h-2V9h2z"/>
                      </svg>
                      <svg class="action-icon" title="Refresh Explorer" @click="${this.refreshRoot}" viewBox="0 0 16 16" fill="currentColor">
                        <path fill-rule="evenodd" clip-rule="evenodd" d="M4.681 3H2V2h3.5l.5.5V6H5V4a5 5 0 1 0 4.53-.761l.302-.953A6 6 0 1 1 4.681 3z"/>
                      </svg>
                    </div>
                  </div>
                  ${this.explorerRoot && this.explorerRoot.isExpanded ? html`
                    <div class="explorer-tree">
                      ${this.explorerRoot.children.map(child => html`
                        <vrutti-explorer .item="${child}"></vrutti-explorer>
                      `)}
                    </div>
                  ` : ''}
                </div>
              `
            : this.activeTab === 'search'
            ? html`<vrutti-search></vrutti-search>`
            : this.activeTab === 'scm'
            ? html`<vrutti-scm></vrutti-scm>`
            : this.activeTab === 'extensions'
            ? html`<vrutti-extensions></vrutti-extensions>`
            : this.activeTab === 'debug'
            ? html`<vrutti-debug-sidebar></vrutti-debug-sidebar>`
            : html`<div style="padding: 15px; opacity: 0.5;">${this.activeTab} panel not yet implemented.</div>`
          }
        </div>
      </div>
      ${this.isOpen ? html`<div class="sidebar-resizer" @mousedown="${this.startResize}"></div>` : ''}
    `;
  }
}
