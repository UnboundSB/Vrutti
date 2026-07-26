import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './vrutti-terminal';

@customElement('vrutti-panel')
export class VruttiPanel extends LitElement {
  @state()
  private activePanelTab = 'TERMINAL';

  @state()
  private terminals: {id: string, name: string}[] = [{id: 'term-1', name: 'bash'}];

  @state()
  private activeTerminalId = 'term-1';

  private nextTerminalId = 2;

  createTerminal() {
    const newId = `term-${this.nextTerminalId++}`;
    this.terminals = [...this.terminals, { id: newId, name: 'bash' }];
    this.setActiveTerminal(newId);
  }

  closeTerminal(id: string, e?: Event) {
    if (e) {
      e.stopPropagation();
    }
    this.terminals = this.terminals.filter(t => t.id !== id);
    if (this.terminals.length === 0) {
      this.dispatchEvent(new Event('close-panel', { bubbles: true, composed: true }));
      // Secretly create a new terminal for next time without opening the panel
      const newId = `term-${this.nextTerminalId++}`;
      this.terminals = [{ id: newId, name: 'bash' }];
      this.activeTerminalId = newId;
    } else if (this.activeTerminalId === id) {
      this.setActiveTerminal(this.terminals[this.terminals.length - 1].id);
    }
  }

  setActiveTerminal(id: string) {
    this.activeTerminalId = id;
    this.focusActiveTerminal();
  }

  public focusActiveTerminal() {
    setTimeout(() => {
      const activeTerm = this.shadowRoot?.querySelector(`vrutti-terminal[id="${this.activeTerminalId}"]`) as any;
      if (activeTerm && typeof activeTerm.focusTerminal === 'function') {
        activeTerm.focusTerminal();
      }
    }, 50);
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      width: 100%;
      height: 100%;
      background: #1a1b26;
      border-top: 1px solid #1f2335;
      position: relative;
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
      color: #717cb4;
      cursor: pointer;
      display: flex;
      align-items: center;
      height: 100%;
      border-bottom: 1px solid transparent;
      margin-right: 8px;
    }
    .panel-top-tab:hover {
      color: #a9b1d6;
    }
    .panel-top-tab.active {
      color: #c0caf5;
      border-bottom: 1px solid #7aa2f7;
    }
    .panel-header-actions {
      display: flex;
      align-items: center;
    }
    .panel-header-actions button {
      background: transparent;
      border: none;
      color: #717cb4;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
      margin-left: 4px;
    }
    .panel-header-actions button:hover {
      background: #292e42;
      color: #c0caf5;
    }
    
    .terminal-body {
      display: flex;
      flex: 1;
      overflow: hidden;
      background: var(--vscode-terminal-background, #1a1b26);
    }
    .terminal-instances {
      flex: 1;
      position: relative;
    }
    .terminal-tabs-container {
      width: 150px;
      border-left: 1px solid #1f2335;
      display: flex;
      flex-direction: column;
      background: #1a1b26;
    }
    .terminal-tabs-actions {
      display: flex;
      justify-content: flex-end;
      padding: 4px;
      border-bottom: 1px solid #1f2335;
    }
    .terminal-tabs-actions button {
      background: transparent;
      border: none;
      color: #a9b1d6;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tabs-actions button:hover {
      background: #292e42;
      color: #c0caf5;
    }
    .terminal-tabs-list {
      flex: 1;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
    }
    .terminal-tab {
      display: flex;
      align-items: center;
      padding: 4px 10px;
      cursor: pointer;
      color: #a9b1d6;
      font-size: 11px;
      user-select: none;
    }
    .terminal-tab:hover {
      background: #292e42;
    }
    .terminal-tab.active {
      color: #c0caf5;
      background: #292e42;
      border-left: 2px solid #7aa2f7;
    }
    .terminal-tab-icon {
      margin-right: 6px;
      display: flex;
      align-items: center;
    }
    .terminal-tab-label {
      flex: 1;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .terminal-tab-close {
      display: none;
      padding: 2px;
      border-radius: 4px;
      color: #a9b1d6;
    }
    .terminal-tab:hover .terminal-tab-close {
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tab-close:hover {
      background: #3b4261;
      color: #c0caf5;
    }
  `;

  render() {
    return html`
      <div class="panel-header">
        <div class="panel-top-tab ${this.activePanelTab === 'PROBLEMS' ? 'active' : ''}" @click=${() => this.activePanelTab = 'PROBLEMS'}>PROBLEMS</div>
        <div class="panel-top-tab ${this.activePanelTab === 'OUTPUT' ? 'active' : ''}" @click=${() => this.activePanelTab = 'OUTPUT'}>OUTPUT</div>
        <div class="panel-top-tab ${this.activePanelTab === 'DEBUG CONSOLE' ? 'active' : ''}" @click=${() => this.activePanelTab = 'DEBUG CONSOLE'}>DEBUG CONSOLE</div>
        <div class="panel-top-tab ${this.activePanelTab === 'TERMINAL' ? 'active' : ''}" @click=${() => {
          this.activePanelTab = 'TERMINAL';
          this.focusActiveTerminal();
        }}>TERMINAL</div>
        <div class="panel-top-tab ${this.activePanelTab === 'PORTS' ? 'active' : ''}" @click=${() => this.activePanelTab = 'PORTS'}>PORTS</div>
        <div style="flex: 1;"></div>
        <div class="panel-header-actions">
          <button title="Maximize Panel">
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M3 3h10v10H3V3zm1-1a1 1 0 0 0-1 1v10a1 1 0 0 0 1 1h10a1 1 0 0 0 1-1V3a1 1 0 0 0-1-1H4z"/></svg>
          </button>
          <button title="Close Panel" @click=${() => this.dispatchEvent(new Event('close-panel', { bubbles: true, composed: true }))}>
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M4.28 3.22a.75.75 0 0 0-1.06 1.06L6.94 8l-3.72 3.72a.75.75 0 1 0 1.06 1.06L8 9.06l3.72 3.72a.75.75 0 1 0 1.06-1.06L9.06 8l3.72-3.72a.75.75 0 0 0-1.06-1.06L8 6.94 4.28 3.22z"/></svg>
          </button>
        </div>
      </div>
      
      ${this.activePanelTab === 'TERMINAL' ? html`
        <div class="terminal-body">
          <div class="terminal-instances">
            ${this.terminals.map(t => html`
              <div style="display: ${this.activeTerminalId === t.id ? 'block' : 'none'}; height: 100%; width: 100%;">
                <vrutti-terminal id="${t.id}" .terminalId=${t.id}></vrutti-terminal>
              </div>
            `)}
          </div>
          <div class="terminal-tabs-container">
            <div class="terminal-tabs-actions">
              <button title="New Terminal" @click=${this.createTerminal}>
                <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M7.75 2a.75.75 0 0 1 .75.75V7h4.25a.75.75 0 0 1 0 1.5H8.5v4.25a.75.75 0 0 1-1.5 0V8.5H2.75a.75.75 0 0 1 0-1.5H7V2.75A.75.75 0 0 1 7.75 2Z"/></svg>
              </button>
              <button title="Kill Terminal" @click=${() => this.closeTerminal(this.activeTerminalId)}>
                <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M11 1.75V3h2.25a.75.75 0 0 1 0 1.5H2.75a.75.75 0 0 1 0-1.5H5V1.75C5 .784 5.784 0 6.75 0h2.5C10.216 0 11 .784 11 1.75ZM4.496 6.675l.66 6.6a.25.25 0 0 0 .249.225h5.19a.25.25 0 0 0 .249-.225l.66-6.6a.75.75 0 0 1 1.492.149l-.66 6.6A1.748 1.748 0 0 1 10.595 15h-5.19a1.75 1.75 0 0 1-1.741-1.575l-.66-6.6a.75.75 0 1 1 1.492-.15ZM6.5 1.75V3h3V1.75a.25.25 0 0 0-.25-.25h-2.5a.25.25 0 0 0-.25.25Z"/></svg>
              </button>
            </div>
            <div class="terminal-tabs-list">
              ${this.terminals.map(t => html`
                <div class="terminal-tab ${this.activeTerminalId === t.id ? 'active' : ''}" @click=${() => this.setActiveTerminal(t.id)}>
                  <div class="terminal-tab-icon">
                    <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2.5 1A1.5 1.5 0 0 0 1 2.5v11A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-11A1.5 1.5 0 0 0 13.5 1h-11zm.5 13V2h10v12H3zm2.5-9v1h5V5H5zm0 3v1h5V8H5z"/></svg>
                  </div>
                  <div class="terminal-tab-label">${t.name}</div>
                  <div class="terminal-tab-close" @click=${(e: Event) => this.closeTerminal(t.id, e)}>
                    <svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor"><path d="M12.28 4.78a.75.75 0 0 0-1.06-1.06L8 6.94 4.78 3.72a.75.75 0 0 0-1.06 1.06L6.94 8l-3.22 3.22a.75.75 0 1 0 1.06 1.06L8 9.06l3.22 3.22a.75.75 0 1 0 1.06-1.06L9.06 8l3.22-3.22z"/></svg>
                  </div>
                </div>
              `)}
            </div>
          </div>
        </div>
      ` : html`
        <div style="flex: 1; display: flex; align-items: center; justify-content: center; color: #717cb4; font-size: 13px;">
          ${this.activePanelTab} - Not yet implemented
        </div>
      `}
    `;
  }
}
