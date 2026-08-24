import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { ExplorerModel, ExplorerItem } from './explorer/explorerModel';
import './explorer/vrutti-explorer';
import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-explorer-view')
export class VruttiExplorerView extends LitElement {
  @state()
  private explorerRoot?: ExplorerItem;

  async connectedCallback() {
    super.connectedCallback();
    window.addEventListener('workspace-changed', this.handleWorkspaceChanged as EventListener);
    window.addEventListener('explorer-refresh', this.handleExplorerRefresh as EventListener);
    
    // Load initial workspace if already set
    const currentWorkspace = (window as any).currentWorkspace;
    if (currentWorkspace) {
      let folderName = currentWorkspace.split(/[\\/]/).pop() || 'Workspace';
      await this.loadWorkspace(currentWorkspace, folderName);
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('workspace-changed', this.handleWorkspaceChanged as EventListener);
    window.removeEventListener('explorer-refresh', this.handleExplorerRefresh as EventListener);
  }

  private handleExplorerRefresh = async () => {
    if (this.explorerRoot) {
      this.explorerRoot.childrenLoaded = false;
      await this.explorerRoot.loadChildren();
      this.requestUpdate();
    }
  };

  private handleWorkspaceChanged = async (e: CustomEvent) => {
    if (e.detail && e.detail.path) {
      let folderName = e.detail.path.split(/[\\/]/).pop() || 'Workspace';
      await this.loadWorkspace(e.detail.path, folderName);
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
    window.dispatchEvent(new CustomEvent('workspace-loaded', { detail: { path } }));
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
      flex-direction: column;
      height: 100%;
      color: var(--vrutti-text, #636b95);
    }

    .workspace-section {
      display: flex;
      flex-direction: column;
      height: 100%;
    }

    .workspace-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 4px 8px;
      cursor: pointer;
      user-select: none;
      background: var(--vrutti-bg, #0f111a);
      border-bottom: 1px solid var(--vrutti-surface-border, #23273b);
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

    .explorer-tree {
      flex: 1;
      overflow-y: auto;
      overflow-x: hidden;
    }
  `];

  render() {
    return html`
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
    `;
  }
}
