import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { unsafeHTML } from 'lit/directives/unsafe-html.js';
import { icon_close, icon_chevron_left, icon_chevron_right } from './codicons';
import './vrutti-explorer-view';
import './vrutti-search';
import './scm/vrutti-scm';
import './vrutti-extensions';
import './vrutti-debug-sidebar';
import './vrutti-webview';
import { registry, ActivityBarContribution, ViewContribution } from '../core/Registry';

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
  private sidebarWidth = 250;

  @state()
  private activityBarItems: ActivityBarContribution[] = [];

  @state()
  private currentViews: ViewContribution[] = [];

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
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    registry.addEventListener('change', this.handleRegistryChange);
    this.updateFromRegistry();
    
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
      window.dispatchEvent(new CustomEvent('workspace-changed', { detail: { path: initialPath } }));
    } else {
      (window as any).currentWorkspace = '';
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('workspace-changed', this.handleWorkspaceChanged);
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    registry.removeEventListener('change', this.handleRegistryChange);
  }

  private handleRegistryChange = (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && (detail.type === 'activitybar' || detail.type === 'views')) {
      this.updateFromRegistry();
    }
  };

  private updateFromRegistry() {
    this.activityBarItems = registry.getActivityBarItems();
    this.currentViews = registry.getViews(this.activeTab);
  }

  private handleIpc = (_e: CustomEvent) => {
  };

  private handleWorkspaceChanged = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail.path) {
      (window as any).currentWorkspace = detail.path;
      localStorage.setItem('lastWorkspace', detail.path);
    }
  };

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
    
    :host(.docked) .activity-bar {
      display: none;
    }
    
    :host(.docked) .sidebar-pane {
      display: none;
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
    this.updateFromRegistry();
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
          ${this.activityBarItems.map(item => html`
            <button class="icon-button ${this.activeTab === item.id ? 'active' : ''}" @click="${() => this.selectTab(item.id)}" title="${item.title}">
              ${item.iconContent ? unsafeSVG(item.iconContent) : html`<svg viewBox="0 0 16 16" width="24" height="24" fill="currentColor"><path d="M1 3h14v10H1V3zm1 1v8h12V4H2zm3 2h6v1H5V6zm0 2h6v1H5V8z"/></svg>`}
            </button>
          `)}
        </div>
        <div class="bottom-icons">
        </div>
      </div>
      <div class="dock-toggle ${this.isDocked ? 'collapsed' : ''}" @click="${() => this.toggleDock()}">
        ${this.isDocked ? unsafeSVG(icon_chevron_right) : unsafeSVG(icon_chevron_left)}
      </div>
      <div class="sidebar-pane ${this.isOpen ? '' : 'collapsed'} ${this.isResizing ? 'resizing' : ''}" style="width: ${this.isOpen ? this.sidebarWidth : 0}px;">
        <div class="pane-header">
          <span>${this.activeTab}</span>
          <button class="pane-action-btn" @click="${() => this.isOpen = false}" title="Minimize">
            ${unsafeSVG(icon_close)}
          </button>
        </div>
        <div class="pane-content" style="padding: 0; overflow-y: auto; overflow-x: hidden;">
          ${this.currentViews.length > 0
            ? html`
                <div class="custom-views-container" style="display: flex; flex-direction: column; height: 100%;">
                  ${this.currentViews.map(view => html`
                    <div class="custom-view" style="flex: 1; border-bottom: 1px solid var(--vrutti-surface-border, #23273b); display: flex; flex-direction: column;">
                      <div class="custom-view-header" style="padding: 4px 8px; font-weight: bold; font-size: 11px; background: var(--vrutti-bg, #0f111a); border-bottom: 1px solid var(--vrutti-surface-border, #23273b);">
                        ${view.name}
                      </div>
                      <div class="custom-view-content" style="flex: 1; position: relative; min-height: 0;">
                        ${view.component === 'vrutti-webview' ? html`<vrutti-webview .viewId=${view.id}></vrutti-webview>` : unsafeHTML(`<${view.component}></${view.component}>`)}
                      </div>
                    </div>
                  `)}
                </div>
              `
            : html`<div style="padding: 15px; opacity: 0.5;">${this.activeTab} panel not yet implemented.</div>`
          }
        </div>
      </div>
      ${this.isOpen ? html`<div class="sidebar-resizer" @mousedown="${this.startResize}"></div>` : ''}
    `;
  }
}
