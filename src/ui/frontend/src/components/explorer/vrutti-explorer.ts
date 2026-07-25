import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { ExplorerItem } from './explorerModel';
import { getIconForFile } from './iconMapper';

import { globalHoverStyle } from '../../shared-styles';

@customElement('vrutti-explorer')
export class VruttiExplorer extends LitElement {
  @property({ type: Object })
  item!: ExplorerItem;

  @property({ type: Number })
  depth: number = 0;

  static styles = [globalHoverStyle, css`
    :host {
      display: block;
      font-family: var(--vrutti-font, 'Inter', -apple-system, sans-serif);
      font-size: 13px;
    }
    
    .tree-node {
      display: flex;
      align-items: center;
      padding: 4px 0;
      cursor: pointer;
      user-select: none;
      color: var(--vrutti-text);
      transition: background 0.1s ease, color 0.1s ease;
      white-space: nowrap;
    }
    
    .tree-node:hover {
      color: var(--vrutti-text-bright);
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
    
    .icon {
      width: 16px;
      height: 16px;
      margin-right: 6px;
      display: inline-block;
      user-select: none;
      -webkit-user-drag: none;
    }
    
    .tree-node .actions {
      display: none;
      margin-left: auto;
      padding-right: 8px;
    }
    
    .tree-node:hover .actions {
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
    
    .children {
      display: block;
    }
  `];

  private async toggle() {
    if (this.item && this.item.isDirectory) {
      await this.item.toggle();
      this.requestUpdate();
    }
  }

  private async createFile(e: Event) {
    e.stopPropagation();
    const name = window.prompt("Enter new file name:");
    if (!name) return;
    
    let basePath = this.item.resource;
    if (basePath.startsWith('file:///')) basePath = basePath.substring(8);
    else if (basePath.startsWith('file://')) basePath = basePath.substring(7);
    
    const targetPath = basePath + "/" + name;
    if ((window as any).vruttiCreateFile) {
      await (window as any).vruttiCreateFile(targetPath);
      // Reload children
      this.item.childrenLoaded = false;
      if (this.item.isExpanded) {
        await this.item.loadChildren();
        this.requestUpdate();
      }
    }
  }

  private async createFolder(e: Event) {
    e.stopPropagation();
    const name = window.prompt("Enter new folder name:");
    if (!name) return;
    
    let basePath = this.item.resource;
    if (basePath.startsWith('file:///')) basePath = basePath.substring(8);
    else if (basePath.startsWith('file://')) basePath = basePath.substring(7);
    
    const targetPath = basePath + "/" + name;
    if ((window as any).vruttiCreateFolder) {
      await (window as any).vruttiCreateFolder(targetPath);
      // Reload children
      this.item.childrenLoaded = false;
      if (this.item.isExpanded) {
        await this.item.loadChildren();
        this.requestUpdate();
      }
    }
  }

  private renderIcon() {
    const iconPath = getIconForFile(this.item.name, this.item.isDirectory, this.item.isExpanded);
    
    let inlineStyle = "";
    // Only color code normal folders based on depth
    if (this.item.isDirectory && (iconPath === './icons/folder.svg' || iconPath === './icons/folder-open.svg')) {
      // Start shift at depth+1 so even root folders get a distinct color from the default
      const hueShift = ((this.depth + 1) * 75) % 360;
      if (hueShift > 0) {
        inlineStyle = `filter: hue-rotate(${hueShift}deg);`;
      }
    }
    
    return html`<img class="icon" src="${iconPath}" style="${inlineStyle}" />`;
  }

  private renderChevron() {
    if (!this.item.isDirectory) return html`<div class="chevron"></div>`;
    return html`
      <div class="chevron ${this.item.isExpanded ? 'expanded' : ''}">
        <svg viewBox="0 0 24 24" width="16" height="16" fill="currentColor">
          <path d="M8.59 16.59L13.17 12 8.59 7.41 10 6l6 6-6 6-1.41-1.41z"/>
        </svg>
      </div>
    `;
  }

  render() {
    if (!this.item) return html``;
    
    const paddingLeft = this.depth * 12 + 8;
    
    return html`
      <div class="tree-node" style="padding-left: ${paddingLeft}px" @click="${this.toggle}">
        ${this.renderChevron()}
        ${this.renderIcon()}
        <span>${this.item.name}</span>
        ${this.item.isDirectory ? html`
          <div class="actions">
            <svg class="action-icon" title="New File" @click="${this.createFile}" viewBox="0 0 16 16" fill="currentColor">
              <path d="M9 1H4v14h9V6H9V1zm0 1.414L11.586 5H9V2.414zM4 0h6l5 5v10H4V0z"/>
              <path d="M5.5 8.5h4v1h-4zm0 2h4v1h-4zm0 2h4v1h-4z"/>
            </svg>
            <svg class="action-icon" title="New Folder" @click="${this.createFolder}" viewBox="0 0 16 16" fill="currentColor">
              <path d="M14 4.5V14H2V3h3.5l1.5 1.5H14zm-1 8.5V5.5H7.5L6 4H3v9h10z"/>
              <path d="M7.5 7h1v2h2v1h-2v2h-1v-2h-2V9h2z"/>
            </svg>
          </div>
        ` : ''}
      </div>
      ${this.item.isDirectory && this.item.isExpanded ? html`
        <div class="children">
          ${this.item.children.map(child => html`
            <vrutti-explorer .item="${child}" .depth="${this.depth + 1}"></vrutti-explorer>
          `)}
        </div>
      ` : ''}
    `;
  }
}
