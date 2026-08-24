import { LitElement, css, html } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';
import { registry, MenuContribution, MenuItemContribution } from '../core/Registry';
import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-context-menu')
export class VruttiContextMenu extends LitElement {
  @property({ type: Number })
  x = 0;

  @property({ type: Number })
  y = 0;

  @property({ type: String })
  menuId = '';

  @property({ type: Object })
  context: any = {};

  @state()
  private menu?: MenuContribution;

  connectedCallback() {
    super.connectedCallback();
    this.menu = registry.getMenu(this.menuId);
    
    // Add event listener to close if clicking outside
    document.addEventListener('mousedown', this.handleDocumentClick);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('mousedown', this.handleDocumentClick);
  }

  private handleDocumentClick = (e: MouseEvent) => {
    const path = e.composedPath();
    if (!path.includes(this)) {
      this.dispatchEvent(new CustomEvent('close-context-menu', { bubbles: true, composed: true }));
    }
  };

  private handleItemClick(item: MenuItemContribution, e: MouseEvent) {
    e.stopPropagation();
    if (item.separator) return;

    this.dispatchEvent(new CustomEvent('close-context-menu', { bubbles: true, composed: true }));
    
    if (item.command) {
        registry.executeCommand(item.command, this.context);
    } else {
        console.warn(`No command assigned to menu item: ${item.label}`);
    }
  }

  private isItemVisible(item: MenuItemContribution): boolean {
    if (!item.when) return true;
    
    // Simple expression evaluator for 'when'
    let expr = item.when.trim();
    const negate = expr.startsWith('!');
    if (negate) expr = expr.substring(1);
    
    const val = !!this.context[expr];
    return negate ? !val : val;
  }

  static styles = [globalHoverStyle, css`
    :host {
      display: block;
      position: fixed;
      z-index: 9999;
    }

    .context-menu {
      min-width: 160px;
      background: var(--vrutti-surface, hsl(230, 25%, 15%));
      backdrop-filter: var(--vrutti-backdrop-filter, blur(10px));
      -webkit-backdrop-filter: var(--vrutti-backdrop-filter, blur(10px));
      border: 1px solid var(--vrutti-surface-border, #23273b);
      border-radius: 6px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
      padding: 6px 0;
      display: flex;
      flex-direction: column;
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      font-size: 13px;
    }

    .context-menu-item {
      padding: 6px 24px 6px 16px;
      color: var(--vrutti-text, #a6accd);
      cursor: pointer;
      user-select: none;
      display: flex;
      align-items: center;
      transition: background-color 0.05s, color 0.05s;
    }

    .context-menu-item:hover {
      background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.1));
      color: var(--vrutti-text-bright, #fff);
    }

    .separator {
      height: 1px;
      background-color: var(--vrutti-surface-border, #23273b);
      margin: 4px 12px;
    }
  `];

  render() {
    if (!this.menu) {
      return html`<div class="context-menu" style="left: ${this.x}px; top: ${this.y}px;">
        <div class="context-menu-item" style="color: var(--vrutti-error, #f07178);">Menu '${this.menuId}' not found</div>
      </div>`;
    }

    const visibleItems = this.menu.items.filter(item => this.isItemVisible(item));

    return html`
      <div class="context-menu" style="left: ${this.x}px; top: ${this.y}px;" @contextmenu=${(e: Event) => e.preventDefault()}>
        ${visibleItems.map(item => item.separator
          ? html`<div class="separator"></div>`
          : html`<div class="context-menu-item" @mousedown="${(e: MouseEvent) => this.handleItemClick(item, e)}">
              ${item.label}
            </div>`
        )}
      </div>
    `;
  }
}
