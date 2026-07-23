import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './explorer/vrutti-explorer';
import { ExplorerModel, ExplorerItem } from './explorer/explorerModel';

@customElement('vrutti-sidebar')
export class VruttiSidebar extends LitElement {
  @state()
  private isOpen = true;

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
      background-color: #13151f; /* slightly darker than main for depth */
      border-right: 1px solid #23273b;
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    }

    .activity-bar {
      width: 48px;
      height: 100%;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding-top: 10px;
      background-color: #181b28;
      border-right: 1px solid #23273b;
      z-index: 10;
    }

    .icon-button {
      background: none;
      border: none;
      color: #82aaff; /* Active color */
      width: 48px;
      height: 48px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      opacity: 0.8;
      transition: opacity 0.2s, background-color 0.2s;
      position: relative;
    }

    .icon-button:hover {
      opacity: 1;
    }

    .icon-button.active {
      opacity: 1;
      color: #82aaff;
    }

    .icon-button.active::before {
      content: '';
      position: absolute;
      left: 0;
      top: 0;
      bottom: 0;
      width: 2px;
      background-color: #82aaff;
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
      padding: 10px 20px;
      font-size: 11px;
      text-transform: uppercase;
      font-weight: 600;
      color: #a6accd;
      letter-spacing: 0.5px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      white-space: nowrap;
    }

    .pane-content {
      padding: 0 10px;
      color: #636b95;
      font-size: 13px;
      white-space: nowrap;
    }

    /* SVG styling */
    svg {
      width: 24px;
      height: 24px;
      fill: currentColor;
    }
  `;

  toggleSidebar() {
    this.isOpen = !this.isOpen;
  }

  render() {
    return html`
      <div class="activity-bar">
        <!-- Files Icon -->
        <button class="icon-button ${this.isOpen ? 'active' : ''}" @click="${this.toggleSidebar}" title="Explorer">
          <svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
            <path d="M13 9h5.5L13 3.5V9M6 2h8l6 6v12a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4c0-1.11.89-2 2-2m0 2v16h12V10h-6V4H6z" />
          </svg>
        </button>
        <!-- Search Icon (Mock) -->
        <button class="icon-button" style="color: #636b95" title="Search">
          <svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
            <path d="M9.5,3A6.5,6.5 0 0,1 16,9.5C16,11.11 15.41,12.59 14.44,13.73L14.71,14H15.5L20.5,19L19,20.5L14,15.5V14.71L13.73,14.44C12.59,15.41 11.11,16 9.5,16A6.5,6.5 0 0,1 3,9.5A6.5,6.5 0 0,1 9.5,3M9.5,5C7,5 5,7 5,9.5C5,12 7,14 9.5,14C12,14 14,12 14,9.5C14,7 12,5 9.5,5Z" />
          </svg>
        </button>
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'}">
        <div class="pane-header">
          EXPLORER
        </div>
        <div class="pane-content" style="padding: 0; overflow-y: auto;">
          <vrutti-explorer .item="${this.explorerRoot}"></vrutti-explorer>
        </div>
      </div>
    `;
  }
}
