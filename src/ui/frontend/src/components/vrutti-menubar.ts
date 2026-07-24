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
        { label: 'New Text File' },
        { label: 'New File...' },
        { label: 'New Window' },
        { separator: true, label: '' },
        { label: 'Open File...' },
        { label: 'Open Folder...' },
        { label: 'Open Workspace from File...' },
        { label: 'Open Recent >' },
        { separator: true, label: '' },
        { label: 'Add Folder to Workspace...' },
        { label: 'Save Workspace As...' },
        { label: 'Duplicate Workspace' },
        { separator: true, label: '' },
        { label: 'Save' },
        { label: 'Save As...' },
        { label: 'Save All' },
        { separator: true, label: '' },
        { label: 'Auto Save' },
        { label: 'Preferences >' },
        { separator: true, label: '' },
        { label: 'Revert File' },
        { label: 'Close Editor' },
        { label: 'Close Folder' },
        { label: 'Close Window' },
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
        { label: 'Replace' },
        { separator: true, label: '' },
        { label: 'Find in Files' },
        { label: 'Replace in Files' },
        { separator: true, label: '' },
        { label: 'Toggle Line Comment' },
        { label: 'Toggle Block Comment' },
        { label: 'Emmet: Expand Abbreviation' }
      ]
    },
    {
      title: 'Selection',
      items: [
        { label: 'Select All' },
        { label: 'Expand Selection' },
        { label: 'Shrink Selection' },
        { separator: true, label: '' },
        { label: 'Copy Line Up' },
        { label: 'Copy Line Down' },
        { label: 'Move Line Up' },
        { label: 'Move Line Down' },
        { label: 'Duplicate Selection' },
        { separator: true, label: '' },
        { label: 'Add Cursor Above' },
        { label: 'Add Cursor Below' },
        { label: 'Add Cursors to Line Ends' },
        { label: 'Add Next Occurrence' },
        { label: 'Add Previous Occurrence' },
        { label: 'Select All Occurrences' },
        { separator: true, label: '' },
        { label: 'Switch to Ctrl+Click for Multi-Cursor' },
        { label: 'Column Selection Mode' }
      ]
    },
    {
      title: 'View',
      items: [
        { label: 'Command Palette...' },
        { label: 'Open View...' },
        { separator: true, label: '' },
        { label: 'Appearance >' },
        { label: 'Editor Layout >' },
        { separator: true, label: '' },
        { label: 'Explorer' },
        { label: 'Search' },
        { label: 'Source Control' },
        { label: 'Run' },
        { label: 'Extensions' },
        { separator: true, label: '' },
        { label: 'Problems' },
        { label: 'Output' },
        { label: 'Debug Console' },
        { label: 'Terminal' },
        { separator: true, label: '' },
        { label: 'Word Wrap' }
      ]
    },
    {
      title: 'Go',
      items: [
        { label: 'Back' },
        { label: 'Forward' },
        { separator: true, label: '' },
        { label: 'Go to File...' },
        { label: 'Go to Symbol in Workspace...' },
        { separator: true, label: '' },
        { label: 'Go to Line/Column...' },
        { label: 'Go to Definition' },
        { label: 'Go to Declaration' },
        { label: 'Go to Type Definition' },
        { label: 'Go to Implementations' },
        { label: 'Go to References' },
        { separator: true, label: '' },
        { label: 'Next Problem' },
        { label: 'Previous Problem' },
        { label: 'Next Change' },
        { label: 'Previous Change' }
      ]
    },
    {
      title: 'Run',
      items: [
        { label: 'Start Debugging' },
        { label: 'Run Without Debugging' },
        { label: 'Stop Debugging' },
        { label: 'Restart Debugging' },
        { separator: true, label: '' },
        { label: 'Open Configurations' },
        { label: 'Add Configuration...' },
        { separator: true, label: '' },
        { label: 'Step Over' },
        { label: 'Step Into' },
        { label: 'Step Out' },
        { label: 'Continue' },
        { separator: true, label: '' },
        { label: 'Toggle Breakpoint' },
        { label: 'New Breakpoint >' }
      ]
    },
    {
      title: 'Terminal',
      items: [
        { label: 'New Terminal' },
        { label: 'Split Terminal' },
        { separator: true, label: '' },
        { label: 'Run Task...' },
        { label: 'Build Task...' },
        { label: 'Active Tasks...' },
        { separator: true, label: '' },
        { label: 'Configure Tasks...' },
        { label: 'Configure Default Build Task...' }
      ]
    },
    {
      title: 'Help',
      items: [
        { label: 'Welcome' },
        { label: 'Show All Commands' },
        { label: 'Documentation' },
        { label: 'Editor Playground' },
        { label: 'Release Notes' },
        { separator: true, label: '' },
        { label: 'Keyboard Shortcuts Reference' },
        { label: 'Video Tutorials' },
        { label: 'Tips and Tricks' },
        { separator: true, label: '' },
        { label: 'Join Us on YouTube' },
        { label: 'Search Feature Requests' },
        { label: 'Report Issue' },
        { separator: true, label: '' },
        { label: 'View License' },
        { label: 'Privacy Statement' },
        { separator: true, label: '' },
        { label: 'Toggle Developer Tools' },
        { label: 'Open Process Explorer' },
        { separator: true, label: '' },
        { label: 'Check for Updates...' },
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

  static styles = css`
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
      background: var(--vrutti-surface, #13151f);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
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
      background-color: var(--vrutti-accent, #2a2d42);
      color: var(--vrutti-text-bright, #fff);
    }

    .separator {
      height: 1px;
      background-color: var(--vrutti-surface-border, #23273b);
      margin: 4px 12px;
    }
  `;

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
