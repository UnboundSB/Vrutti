import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface MenuItem {
  label: string;
  action?: string;
  separator?: boolean;
}

interface MenuData {
  title: string;
  items: MenuItem[];
}

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-menubar')
export class VruttiMenuBar extends LitElement {
  @state()
  private activeMenu: number | null = null;

  @state()
  private isMenuOpen = false;

  private menus: MenuData[] = [
    {
      title: 'File',
      items: [
        { label: 'New File' },
        { label: 'Open File' },
        { label: 'Open Folder...', action: 'openFolder' },
        { separator: true, label: '' },
        { label: 'Save' },
        { label: 'Save As' },
        { separator: true, label: '' },
        { label: 'Preferences' },
        { separator: true, label: '' },
        { label: 'Exit', action: 'closeWindow' }
      ]
    },
    {
      title: 'Edit',
      items: [
        { label: 'Undo' },
        { label: 'Redo' },
        { separator: true, label: '' },
        { label: 'Cut' },
        { label: 'Copy' },
        { label: 'Paste' },
        { separator: true, label: '' },
        { label: 'Find' },
        { label: 'Replace' }
      ]
    },
    {
      title: 'View',
      items: [
        { label: 'Command Palette' },
        { separator: true, label: '' },
        { label: 'Explorer' },
        { label: 'Search' },
        { label: 'Source Control' },
        { label: 'Extensions' },
        { separator: true, label: '' },
        { label: 'Terminal', action: 'toggleTerminal' },
        { label: 'Word Wrap' }
      ]
    },
    {
      title: 'Help',
      items: [
        { label: 'Welcome' },
        { label: 'Keyboard Shortcuts Reference' },
        { separator: true, label: '' },
        { label: 'Toggle Developer Tools' },
        { separator: true, label: '' },
        { label: 'About' }
      ]
    }
  ];

  connectedCallback() {
    super.connectedCallback();
    document.addEventListener('mousedown', this.handleDocumentClick);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('mousedown', this.handleDocumentClick);
  }

  private handleDocumentClick = (e: MouseEvent) => {
    const path = e.composedPath();
    if (!path.includes(this)) {
      this.closeMenu();
    }
  };

  private closeMenu() {
    this.isMenuOpen = false;
    this.activeMenu = null;
  }

  private handleMenuClick(index: number, e: MouseEvent) {
    e.stopPropagation();
    if (this.isMenuOpen && this.activeMenu === index) {
      this.closeMenu();
    } else {
      this.isMenuOpen = true;
      this.activeMenu = index;
    }
  }

  private handleMenuHover(index: number) {
    if (this.isMenuOpen) {
      this.activeMenu = index;
    }
  }

  private handleItemClick(item: MenuItem, e: MouseEvent) {
    e.stopPropagation();
    if (item.separator) return;
    
    this.closeMenu();
    
    if (item.action === 'closeWindow' && (window as any).closeWindow) {
      (window as any).closeWindow();
    } else {
      console.log(`Action: ${item.label}`);
      // Dispatch custom event for app to handle
      this.dispatchEvent(new CustomEvent('menu-action', {
        detail: { action: item.action || item.label },
        bubbles: true,
        composed: true
      }));
    }
  }

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      align-items: center;
      height: 100%;
      -webkit-app-region: no-drag;
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      font-size: 13px;
    }

    .menu-bar {
      display: flex;
      height: 100%;
    }

    .menu-container {
      position: relative;
      display: flex;
      align-items: center;
    }

    .menu-header {
      padding: 4px 8px;
      margin: 0 2px;
      color: #a6accd;
      cursor: default;
      border-radius: 4px;
      user-select: none;
      transition: background-color 0.1s, color 0.1s;
    }

    .menu-header:hover, .menu-container.active .menu-header {
      background: var(--vrutti-surface-border, rgba(255, 255, 255, 0.1));
      color: var(--vrutti-text-bright, #fff);
    }

    .dropdown {
      position: absolute;
      top: 100%;
      left: 2px;
      min-width: 220px;
      background: hsl(230, 25%, 15%);
      border: 1px solid var(--vrutti-surface-border, #23273b);
      border-radius: 6px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
      padding: 6px 0;
      z-index: 1000;
      display: flex;
      flex-direction: column;
    }

    .dropdown-item {
      padding: 6px 24px 6px 32px;
      color: var(--vrutti-text, #a6accd);
      cursor: pointer;
      user-select: none;
      display: flex;
      align-items: center;
      transition: background-color 0.05s, color 0.05s;
    }

    .dropdown-item:hover {
      color: var(--vrutti-text-bright, #fff);
    }

    .separator {
      height: 1px;
      background-color: var(--vrutti-surface-border, #23273b);
      margin: 4px 12px;
    }
  `];

  render() {
    return html`
      <div class="menu-bar">
        ${this.menus.map((menu, index) => html`
          <div class="menu-container ${this.isMenuOpen && this.activeMenu === index ? 'active' : ''}">
            <div 
              class="menu-header" 
              @mousedown="${(e: MouseEvent) => this.handleMenuClick(index, e)}"
              @mouseenter="${() => this.handleMenuHover(index)}"
            >
              ${menu.title}
            </div>
            
            ${this.isMenuOpen && this.activeMenu === index ? html`
              <div class="dropdown">
                ${menu.items.map(item => item.separator 
                  ? html`<div class="separator"></div>`
                  : html`<div class="dropdown-item" @mousedown="${(e: MouseEvent) => this.handleItemClick(item, e)}">
                      ${item.label}
                    </div>`
                )}
              </div>
            ` : ''}
          </div>
        `)}
      </div>
    `;
  }
}
