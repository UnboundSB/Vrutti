import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { ExplorerItem } from './explorerModel';

@customElement('vrutti-explorer')
export class VruttiExplorer extends LitElement {
  @property({ type: Object })
  item!: ExplorerItem;

  @property({ type: Number })
  depth: number = 0;

  static styles = css`
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
      background: var(--vrutti-surface-border);
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
      fill: currentColor;
      opacity: 0.9;
    }
    
    .children {
      display: block;
    }
  `;

  private toggle() {
    if (this.item && this.item.isDirectory) {
      this.item.toggle();
      this.requestUpdate();
    }
  }

  private renderIcon() {
    if (this.item.isDirectory) {
      return html`
        <svg class="icon" viewBox="0 0 24 24">
          <path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"/>
        </svg>
      `;
    }
    return html`
      <svg class="icon" viewBox="0 0 24 24">
        <path d="M6 2c-1.1 0-1.99.9-1.99 2L4 20c0 1.1.89 2 1.99 2H18c1.1 0 2-.9 2-2V8l-6-6H6zm7 7V3.5L18.5 9H13z"/>
      </svg>
    `;
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
