import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeHTML } from 'lit/directives/unsafe-html.js';
import './vrutti-terminal-view';
import './vrutti-output-view';
import './vrutti-debug-console';
import './vrutti-webview';
import { registry, PanelTabContribution } from '../core/Registry';

@customElement('vrutti-panel')
export class VruttiPanel extends LitElement {
  @state()
  private activePanelTab = 'TERMINAL';

  @state()
  private panelTabs: PanelTabContribution[] = [];

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('switch-to-debug-console', this.handleSwitchToDebugConsole as EventListener);
    registry.addEventListener('change', this.handleRegistryChange);
    this.updateFromRegistry();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('switch-to-debug-console', this.handleSwitchToDebugConsole as EventListener);
    registry.removeEventListener('change', this.handleRegistryChange);
  }

  private handleRegistryChange = (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && detail.type === 'paneltabs') {
      this.updateFromRegistry();
    }
  };

  private updateFromRegistry() {
    this.panelTabs = registry.getPanelTabs();
  }

  private handleSwitchToDebugConsole = () => {
    this.activePanelTab = 'DEBUG CONSOLE';
  };

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'webview/createPanel') {
      const panel = msg.params;
      registry.registerPanelTab({
        id: panel.id,
        title: panel.title || 'Webview',
        component: 'vrutti-webview'
      });
      this.activePanelTab = panel.id;
    } else if (msg && msg.method === 'webview/dispose') {
      registry.removePanelTab(msg.params.id);
      if (this.activePanelTab === msg.params.id) {
        this.activePanelTab = 'TERMINAL';
      }
    }
  };

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      width: 100%;
      height: 100%;
      background: var(--vrutti-surface, #1a1b26);
      border-top: 1px solid var(--vrutti-surface-border, #1f2335);
      position: relative;
      min-width: 0;
      overflow: hidden;
    }
    
    .panel-header {
      display: flex;
      align-items: center;
      height: 35px;
      padding: 0 8px;
      user-select: none;
    }
    .panel-top-tab {
      padding: 0 10px;
      font-size: 11px;
      text-transform: uppercase;
      font-weight: 500;
      color: var(--vrutti-text, #717cb4);
      cursor: pointer;
      display: flex;
      align-items: center;
      height: 100%;
      border-bottom: 1px solid transparent;
      margin-right: 8px;
    }
    .panel-top-tab:hover {
      color: var(--vrutti-text-bright, #a9b1d6);
    }
    .panel-top-tab.active {
      color: var(--vrutti-text-bright, #c0caf5);
      border-bottom: 1px solid var(--vrutti-accent, #7aa2f7);
    }
    .panel-header-actions {
      display: flex;
      align-items: center;
    }
    .panel-header-actions button {
      background: transparent;
      border: none;
      color: var(--vrutti-text, #717cb4);
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      margin-left: 4px;
    }
    .panel-header-actions button:hover {
      background: var(--vrutti-surface-border, #292e42);
      color: var(--vrutti-text-bright, #c0caf5);
    }
    
    .panel-body {
      display: flex;
      flex: 1;
      overflow: hidden;
      min-width: 0;
      background: var(--vrutti-bg, #1a1b26);
    }
  `;

  render() {
    return html`
      <div class="panel-header">
        ${this.panelTabs.map(tab => html`
          <div class="panel-top-tab ${this.activePanelTab === tab.id ? 'active' : ''}" @click=${() => {
            this.activePanelTab = tab.id;
            if (tab.id === 'TERMINAL') {
              this.focusActiveTerminal();
            }
          }}>${tab.title.toUpperCase()}</div>
        `)}
        <div style="flex: 1;"></div>
        ${this.activePanelTab === 'OUTPUT' ? html`
          <div class="output-channel-selector">
            <select @change=${(e: Event) => this.activeOutputChannel = (e.target as HTMLSelectElement).value}>
              ${this.outputChannels.map(c => html`
                <option value="${c}" ?selected=${this.activeOutputChannel === c}>${c}</option>
              `)}
            </select>
          </div>
        ` : this.activePanelTab === 'DEBUG CONSOLE' ? html`
          <div class="output-channel-selector" style="margin-right: 12px; display: flex; align-items: center; gap: 8px;">
            <select style="background: #1a1b26; color: #a9b1d6; border: 1px solid #1f2335; padding: 2px 6px; border-radius: 4px; font-size: 11px; outline: none; cursor: pointer;">
              <option value="node">Node.js - Extension Host</option>
              <option value="cpp">C++ (gdb) - vrutti</option>
            </select>
            <select style="background: #1a1b26; color: #a9b1d6; border: 1px solid #1f2335; padding: 2px 6px; border-radius: 4px; font-size: 11px; outline: none; cursor: pointer;">
              <option value="t1">Thread 1 (Main)</option>
              <option value="t2">Thread 2 (Worker)</option>
            </select>
          </div>
        ` : ''}
        <div class="panel-header-actions">
          <button title="Maximize Panel">
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M3 3h10v10H3V3zm1-1a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h10a1 1 0 0 0 1-1V3a1 1 0 0 0-1-1H4z"/></svg>
          </button>
          <button title="Close Panel" @click=${() => this.dispatchEvent(new Event('close-panel', { bubbles: true, composed: true }))}>
          </div>
        </div>
      ` : this.activePanelTab === 'DEBUG CONSOLE' ? html`
        <vrutti-debug-console></vrutti-debug-console>
      ` : this.activePanelTab === 'PORTS' ? html`
        <div style="padding: 15px; opacity: 0.5;">Ports panel not yet implemented.</div>
      ` : this.panelTabs.find(p => p.id === this.activePanelTab)?.component === 'vrutti-webview' ? html`
        <div style="flex: 1; display: flex; flex-direction: column; background: var(--vscode-editor-background, #1a1b26);">
          <vrutti-webview .webviewId=${this.activePanelTab}></vrutti-webview>
        </div>
      ` : html`
        <div style="flex: 1; display: flex; align-items: center; justify-content: center; color: #717cb4; font-size: 13px;">
          ${this.activePanelTab} - Not yet implemented
        </div>
      `}
    `;
  }
}
