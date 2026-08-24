import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './vrutti-terminal';

interface TerminalGroup {
  id: string;
  name: string;
  activeTerminalId: string;
  terminals: { id: string, name: string }[];
}

@customElement('vrutti-terminal-view')
export class VruttiTerminalView extends LitElement {
  @state()
  private terminalGroups: TerminalGroup[] = [{
    id: 'group-1',
    name: 'bash',
    activeTerminalId: 'term-1',
    terminals: [{ id: 'term-1', name: 'bash' }]
  }];

  @state()
  private activeGroupId = 'group-1';

  private nextGroupId = 2;
  private nextTerminalId = 2;

  private terminalReadyStatus: Record<string, boolean> = {};
  private terminalInputQueues: Record<string, string[]> = {};

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('terminal-ready', this.handleTerminalReady as EventListener);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('terminal-ready', this.handleTerminalReady as EventListener);
  }

  private handleTerminalReady = (e: Event) => {
    const termId = (e as CustomEvent).detail?.id;
    if (termId) {
      this.terminalReadyStatus[termId] = true;
      if (this.terminalInputQueues[termId]) {
        this.terminalInputQueues[termId].forEach(cmd => this.sendTerminalInput(termId, cmd));
        this.terminalInputQueues[termId] = [];
      }
    }
  };

  private sendTerminalInput(termId: string, cmd: string) {
    if ((window as any).vruttiTerminalInput) {
      const b64 = btoa(new TextEncoder().encode(cmd).reduce((data, byte) => data + String.fromCharCode(byte), ''));
      (window as any).vruttiTerminalInput(termId, b64);
    }
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'terminal/runCommand' && msg.params && msg.params.command) {
      const activeGroup = this.terminalGroups.find(g => g.id === this.activeGroupId);
      if (activeGroup && activeGroup.activeTerminalId) {
        const cmd = msg.params.command + "\r";
        const termId = activeGroup.activeTerminalId;
        
        if (this.terminalReadyStatus[termId]) {
          this.sendTerminalInput(termId, cmd);
        } else {
          if (!this.terminalInputQueues[termId]) this.terminalInputQueues[termId] = [];
          this.terminalInputQueues[termId].push(cmd);
        }
      }
    }
  };

  createTerminalGroup() {
    const newGroupId = `group-${this.nextGroupId++}`;
    const newTermId = `term-${this.nextTerminalId++}`;
    const newGroup: TerminalGroup = {
      id: newGroupId,
      name: 'bash',
      activeTerminalId: newTermId,
      terminals: [{ id: newTermId, name: 'bash' }]
    };
    this.terminalGroups = [...this.terminalGroups, newGroup];
    this.setActiveGroup(newGroupId);
  }

  splitTerminalGroup(groupId: string) {
    const group = this.terminalGroups.find(g => g.id === groupId);
    if (group) {
      const newTermId = `term-${this.nextTerminalId++}`;
      const newGroup = { ...group };
      newGroup.terminals = [...newGroup.terminals, { id: newTermId, name: 'bash' }];
      newGroup.name = newGroup.terminals.map(t => t.name).join(', ');
      newGroup.activeTerminalId = newTermId;
      
      this.terminalGroups = this.terminalGroups.map(g => g.id === newGroup.id ? newGroup : g);
      this.resetSplitFlexLayout();
      this.setActiveGroup(groupId);
    }
  }

  closeTerminalGroup(groupId: string, e?: Event) {
    if (e) e.stopPropagation();
    
    this.terminalGroups = this.terminalGroups.filter(g => g.id !== groupId);
    
    if (this.terminalGroups.length === 0) {
      // Just recreate a new default terminal if all are closed
      const newGroupId = `group-${this.nextGroupId++}`;
      const newTermId = `term-${this.nextTerminalId++}`;
      this.terminalGroups = [{
        id: newGroupId,
        name: 'bash',
        activeTerminalId: newTermId,
        terminals: [{ id: newTermId, name: 'bash' }]
      }];
      this.activeGroupId = newGroupId;
    } else if (this.activeGroupId === groupId) {
      this.setActiveGroup(this.terminalGroups[this.terminalGroups.length - 1].id);
    }
  }

  closeSplitTerminal(groupId: string, termId: string, e?: Event) {
    if (e) e.stopPropagation();
    
    let group = this.terminalGroups.find(g => g.id === groupId);
    if (!group) return;

    if (group.terminals.length === 1) {
      this.closeTerminalGroup(groupId);
      return;
    }

    const newGroup = { ...group };
    newGroup.terminals = newGroup.terminals.filter(t => t.id !== termId);
    newGroup.name = newGroup.terminals.map(t => t.name).join(', ');
    
    if (newGroup.activeTerminalId === termId) {
      newGroup.activeTerminalId = newGroup.terminals[newGroup.terminals.length - 1].id;
    }

    this.terminalGroups = this.terminalGroups.map(g => g.id === newGroup.id ? newGroup : g);
    this.resetSplitFlexLayout();

    if (this.activeGroupId === groupId) {
      this.focusActiveTerminal();
    }
  }

  setActiveGroup(id: string) {
    this.activeGroupId = id;
    this.focusActiveTerminal();
  }

  setActiveSplit(groupId: string, termId: string) {
    const group = this.terminalGroups.find(g => g.id === groupId);
    if (group && group.activeTerminalId !== termId) {
      const newGroup = { ...group, activeTerminalId: termId };
      this.terminalGroups = this.terminalGroups.map(g => g.id === newGroup.id ? newGroup : g);
      this.focusActiveTerminal();
    }
  }

  public focusActiveTerminal() {
    setTimeout(() => {
      const activeGroup = this.terminalGroups.find(g => g.id === this.activeGroupId);
      if (activeGroup) {
        const activeTerm = this.shadowRoot?.querySelector(`vrutti-terminal[id="${activeGroup.activeTerminalId}"]`) as any;
        if (activeTerm && typeof activeTerm.focusTerminal === 'function') {
          activeTerm.focusTerminal();
        }
      }
    }, 50);
  }

  // --- Horizontal Split Resizing ---
  private activeResizerIndex = -1;
  private startX = 0;
  private leftStartWidth = 0;
  private rightStartWidth = 0;
  private leftSplitEl: HTMLElement | null = null;
  private rightSplitEl: HTMLElement | null = null;

  private resetSplitFlexLayout() {
    setTimeout(() => {
      const containers = this.shadowRoot?.querySelectorAll('.terminal-split-container');
      containers?.forEach((el: any) => {
        el.style.flex = '1';
        el.style.width = 'auto';
      });
      window.dispatchEvent(new Event('resize'));
    }, 10);
  }

  private startSplitResize = (e: MouseEvent, index: number) => {
    e.preventDefault();
    this.activeResizerIndex = index;
    this.startX = e.clientX;
    
    const containers = this.shadowRoot?.querySelectorAll('.terminal-split-container');
    if (containers && containers.length > index + 1) {
      this.leftSplitEl = containers[index] as HTMLElement;
      this.rightSplitEl = containers[index + 1] as HTMLElement;
      const containerWidth = (this.shadowRoot?.querySelector('.terminal-instances') as HTMLElement).getBoundingClientRect().width;
      
      containers.forEach((el: any) => {
        el.style.flex = 'none';
        const pct = (el.getBoundingClientRect().width / containerWidth) * 100;
        el.style.width = pct + '%';
      });

      this.leftStartWidth = (this.leftSplitEl.getBoundingClientRect().width / containerWidth) * 100;
      this.rightStartWidth = (this.rightSplitEl.getBoundingClientRect().width / containerWidth) * 100;

      window.addEventListener('mousemove', this.doSplitResize);
      window.addEventListener('mouseup', this.stopSplitResize);
      document.body.style.cursor = 'ew-resize';
    }
  }

  private doSplitResize = (e: MouseEvent) => {
    if (this.activeResizerIndex === -1 || !this.leftSplitEl || !this.rightSplitEl) return;
    
    const dx = e.clientX - this.startX;
    const containerWidth = (this.shadowRoot?.querySelector('.terminal-instances') as HTMLElement).getBoundingClientRect().width;
    const dxPct = (dx / containerWidth) * 100;

    let newLeftWidth = this.leftStartWidth + dxPct;
    let newRightWidth = this.rightStartWidth - dxPct;
    
    if (newLeftWidth < 5) {
      newRightWidth -= (5 - newLeftWidth);
      newLeftWidth = 5;
    }
    if (newRightWidth < 5) {
      newLeftWidth -= (5 - newRightWidth);
      newRightWidth = 5;
    }

    this.leftSplitEl.style.width = newLeftWidth + '%';
    this.rightSplitEl.style.width = newRightWidth + '%';
    
    window.dispatchEvent(new Event('resize'));
  };

  private stopSplitResize = () => {
    this.activeResizerIndex = -1;
    this.leftSplitEl = null;
    this.rightSplitEl = null;
    window.removeEventListener('mousemove', this.doSplitResize);
    window.removeEventListener('mouseup', this.stopSplitResize);
    document.body.style.cursor = '';
  };

  static styles = css`
    :host {
      display: flex;
      flex-direction: row;
      width: 100%;
      height: 100%;
      overflow: hidden;
      min-width: 0;
      background: var(--vrutti-bg, #1a1b26);
    }
    
    .terminal-instances {
      flex: 1;
      display: flex;
      flex-direction: row;
      position: relative;
      background: var(--vrutti-bg, #1a1b26);
      min-width: 0;
      overflow: hidden;
    }
    .terminal-split-container {
      flex: 1;
      height: 100%;
      position: relative;
      display: flex;
      flex-direction: column;
      min-width: 0;
    }
    .split-resizer {
      width: 4px;
      cursor: ew-resize;
      background: transparent;
      position: relative;
      z-index: 100;
      transition: background 0.1s;
    }
    .split-resizer:hover, .split-resizer.active {
      background: var(--vrutti-accent, #7aa2f7);
    }
    
    .terminal-split-container.active-split::after {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0; bottom: 0;
      pointer-events: none;
      border: 1px solid rgba(122, 162, 247, 0.3);
      z-index: 10;
    }
    .terminal-tabs-container {
      width: 150px;
      flex-shrink: 0;
      border-left: 1px solid var(--vrutti-surface-border, #1f2335);
      display: flex;
      flex-direction: column;
      background: var(--vrutti-surface, #1a1b26);
    }
    .terminal-tabs-actions {
      display: flex;
      justify-content: flex-end;
      padding: 4px;
      border-bottom: 1px solid var(--vrutti-surface-border, #1f2335);
    }
    .terminal-tabs-actions button {
      background: transparent;
      border: none;
      color: var(--vrutti-text-bright, #a9b1d6);
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tabs-actions button:hover {
      background: var(--vrutti-surface-border, #292e42);
      color: var(--vrutti-text-bright, #c0caf5);
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
      color: var(--vrutti-text-bright, #a9b1d6);
      font-size: 11px;
      user-select: none;
    }
    .terminal-tab:hover {
      background: var(--vrutti-surface-border, #292e42);
    }
    .terminal-tab.active {
      color: var(--vrutti-text-bright, #c0caf5);
      background: var(--vrutti-surface-border, #292e42);
      border-left: 2px solid var(--vrutti-accent, #7aa2f7);
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
    .terminal-tab-split,
    .terminal-tab-close {
      display: none;
      padding: 2px;
      border-radius: 4px;
      color: #a9b1d6;
    }
    .terminal-tab:hover .terminal-tab-split,
    .terminal-tab:hover .terminal-tab-close {
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tab-split:hover,
    .terminal-tab-close:hover {
      background: #3b4261;
      color: #c0caf5;
    }
    .terminal-tab-split svg {
      transform: rotate(90deg);
    }
    
    .split-overlay-actions {
      position: absolute;
      top: 4px;
      right: 14px;
      display: none;
      background: rgba(41, 46, 66, 0.8);
      border-radius: 4px;
      padding: 2px;
      z-index: 50;
    }
    .terminal-split-container:hover .split-overlay-actions {
      display: flex;
    }
    .split-overlay-actions button {
      background: none;
      border: none;
      color: #a9b1d6;
      cursor: pointer;
      padding: 2px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .split-overlay-actions button:hover {
      color: #c0caf5;
    }
  `;

  render() {
    return html`
      <div class="terminal-instances">
        ${this.terminalGroups.map(group => html`
          <div class="terminal-group-wrapper" style="display: ${this.activeGroupId === group.id ? 'flex' : 'none'}; flex: 1; min-width: 0; position: relative;">
            ${group.terminals.map((t, index) => html`
              ${index > 0 ? html`
                <div class="split-resizer ${this.activeResizerIndex === index - 1 ? 'active' : ''}" 
                     @mousedown=${(e: MouseEvent) => this.startSplitResize(e, index - 1)}></div>
              ` : ''}
              <div class="terminal-split-container ${group.activeTerminalId === t.id ? 'active-split' : ''}" @click=${() => this.setActiveSplit(group.id, t.id)}>
                <div class="split-overlay-actions">
                  <button title="Kill Split" @click=${(e: Event) => this.closeSplitTerminal(group.id, t.id, e)}>
                    <svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor"><path d="M12.28 4.78a.75.75 0 0 0-1.06-1.06L8 6.94 4.78 3.72a.75.75 0 0 0-1.06 1.06L6.94 8l-3.22 3.22a.75.75 0 1 0 1.06 1.06L8 9.06l3.22 3.22a.75.75 0 1 0 1.06-1.06L9.06 8l3.22-3.22z"/></svg>
                  </button>
                </div>
                <vrutti-terminal id="${t.id}" .terminalId=${t.id}></vrutti-terminal>
              </div>
            `)}
          </div>
        `)}
      </div>
      <div class="terminal-tabs-container">
        <div class="terminal-tabs-actions">
          <button title="New Terminal" @click=${this.createTerminalGroup}>
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M7.75 2a.75.75 0 0 1 .75.75V7h4.25a.75.75 0 0 1 0 1.5H8.5v4.25a.75.75 0 0 1-1.5 0V8.5H2.75a.75.75 0 0 1 0-1.5H7V2.75A.75.75 0 0 1 7.75 2Z"/></svg>
          </button>
          <button title="Kill Terminal Group" @click=${() => this.closeTerminalGroup(this.activeGroupId)}>
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M11 1.75V3h2.25a.75.75 0 0 1 0 1.5H2.75a.75.75 0 0 1 0-1.5H5V1.75C5 .784 5.784 0 6.75 0h2.5C10.216 0 11 .784 11 1.75ZM4.496 6.675l.66 6.6a.25.25 0 0 0 .249.225h5.19a.25.25 0 0 0 .249-.225l.66-6.6a.75.75 0 0 1 1.492.149l-.66 6.6A1.748 1.748 0 0 1 10.595 15h-5.19a1.75 1.75 0 0 1-1.741-1.575l-.66-6.6a.75.75 0 1 1 1.492-.15ZM6.5 1.75V3h3V1.75a.25.25 0 0 0-.25-.25h-2.5a.25.25 0 0 0-.25.25Z"/></svg>
          </button>
        </div>
        <div class="terminal-tabs-list">
          ${this.terminalGroups.map(g => html`
            <div class="terminal-tab ${this.activeGroupId === g.id ? 'active' : ''}" @click=${() => this.setActiveGroup(g.id)}>
              <div class="terminal-tab-icon">
                ${g.terminals.length > 1 
                  ? html`<svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2.75 2a.75.75 0 0 0-.75.75v10.5c0 .414.336.75.75.75h10.5a.75.75 0 0 0 .75-.75V2.75a.75.75 0 0 0-.75-.75H2.75zM3.5 12.5V3.5h3.5v9H3.5zm5 0V3.5h4v9h-4z"/></svg>`
                  : html`<svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2.5 1A1.5 1.5 0 0 0 1 2.5v11A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-11A1.5 1.5 0 0 0 13.5 1h-11zm.5 13V2h10v12H3zm2.5-9v1h5V5H5zm0 3v1h5V8H5z"/></svg>`
                }
              </div>
              <div class="terminal-tab-label">${g.name}</div>
              <div class="terminal-tab-split" @click=${(e: Event) => { e.stopPropagation(); this.splitTerminalGroup(g.id); }}>
                <svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor"><path d="M2.75 2a.75.75 0 0 0-.75.75v10.5c0 .414.336.75.75.75h10.5a.75.75 0 0 0 .75-.75V2.75a.75.75 0 0 0-.75-.75H2.75zM3.5 12.5V3.5h3.5v9H3.5zm5 0V3.5h4v9h-4z"/></svg>
              </div>
              <div class="terminal-tab-close" @click=${(e: Event) => this.closeTerminalGroup(g.id, e)}>
                <svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor"><path d="M12.28 4.78a.75.75 0 0 0-1.06-1.06L8 6.94 4.78 3.72a.75.75 0 0 0-1.06 1.06L6.94 8l-3.22 3.22a.75.75 0 1 0 1.06 1.06L8 9.06l3.22 3.22a.75.75 0 1 0 1.06-1.06L9.06 8l3.22-3.22z"/></svg>
              </div>
            </div>
          `)}
        </div>
      </div>
    `;
  }
}
