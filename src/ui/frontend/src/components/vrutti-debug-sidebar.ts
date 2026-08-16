import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface Variable {
  name: string;
  value: string;
  type: string;
  hasChildren: boolean;
  expanded?: boolean;
  children?: Variable[];
}

interface CallFrame {
  id: string;
  name: string;
  file: string;
  line: number;
}

interface Breakpoint {
  file: string;
  line: number;
  enabled: boolean;
}

@customElement('vrutti-debug-sidebar')
export class VruttiDebugSidebar extends LitElement {
  @state() private debugState: 'inactive' | 'paused' | 'running' = 'inactive';
  
  // Sections expanded state
  @state() private sections = {
    variables: true,
    watch: true,
    callStack: true,
    breakpoints: true
  };

  @state() private variables: Variable[] = [];
  @state() private watch: string[] = [];
  @state() private callStack: CallFrame[] = [];
  @state() private breakpoints: Breakpoint[] = [];

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  disconnectedCallback() {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    super.disconnectedCallback();
  }

  private handleIpc = (e: CustomEvent) => {
    const data = e.detail;
    if (data.method === 'debug/paused') {
      this.debugState = 'paused';
      this.callStack = data.params.callStack || [];
      this.variables = data.params.variables || [];
      this.watch = data.params.watch || [];
      
      // Update editor execution line
      if (this.callStack.length > 0) {
        const topFrame = this.callStack[0];
        window.dispatchEvent(new CustomEvent('vrutti-debug-pause', {
          detail: { file: topFrame.file, line: topFrame.line }
        }));
      }
    } else if (data.method === 'debug/resumed') {
      this.debugState = 'running';
      this.callStack = [];
      this.variables = [];
      window.dispatchEvent(new CustomEvent('vrutti-debug-resume'));
    }
  };

  private sendDebugCommand(command: string, params: any = {}) {
    if ((window as any).vruttiIpcAsync) {
      (window as any).vruttiIpcAsync(JSON.stringify({
        method: `debug/${command}`,
        params
      }));
    }
  }

  private toggleSection(section: keyof typeof this.sections) {
    this.sections = { ...this.sections, [section]: !this.sections[section] };
  }

  private renderDebugAction(icon: string, label: string, action: () => void) {
    return html`
      <button class="action-btn" title="${label}" @click=${action}>
        ${icon}
      </button>
    `;
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100%;
      color: var(--vrutti-text-bright, #a9b1d6);
      font-size: 13px;
    }

    .debug-toolbar {
      display: flex;
      padding: 8px;
      gap: 4px;
      background: var(--vrutti-surface-border, #1f2335);
      border-bottom: 1px solid #292e42;
      justify-content: center;
    }

    .action-btn {
      background: none;
      border: none;
      color: #7aa2f7;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      font-size: 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      width: 24px;
      height: 24px;
    }

    .action-btn:hover {
      background: #292e42;
      color: #c0caf5;
    }
    
    .action-btn.stop { color: #f7768e; }

    .section {
      display: flex;
      flex-direction: column;
      border-bottom: 1px solid #1f2335;
    }

    .section-header {
      display: flex;
      align-items: center;
      padding: 4px 8px;
      cursor: pointer;
      background: #1a1b26;
      font-weight: bold;
      text-transform: uppercase;
      font-size: 11px;
      color: #717cb4;
    }

    .section-header:hover {
      color: #a9b1d6;
    }

    .section-header svg {
      width: 12px;
      height: 12px;
      margin-right: 4px;
      transition: transform 0.1s;
    }

    .section-header.expanded svg {
      transform: rotate(90deg);
    }

    .section-content {
      display: none;
      padding: 4px 0;
      background: #1a1b26;
    }

    .section-content.expanded {
      display: block;
    }

    .item {
      display: flex;
      align-items: center;
      padding: 2px 8px 2px 24px;
      cursor: pointer;
      font-family: 'Fira Code', 'Cascadia Code', Consolas, monospace;
      font-size: 12px;
    }

    .item:hover {
      background: #1f2335;
    }

    .var-name { color: #7aa2f7; margin-right: 8px; }
    .var-value { color: #9ece6a; }
    
    .frame-name { color: #e0af68; margin-right: 8px; }
    .frame-file { color: #565f89; font-size: 11px; }

    .bp-file { color: #a9b1d6; margin-right: 8px; }
    .bp-line { color: #ff9e64; }
    
    .checkbox {
      width: 12px;
      height: 12px;
      margin-right: 8px;
      border: 1px solid #565f89;
      border-radius: 2px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    
    .checkbox.checked {
      background: #7aa2f7;
      border-color: #7aa2f7;
    }
  `;

  render() {
    return html`
      ${this.debugState !== 'inactive' ? html`
        <div class="debug-toolbar">
          ${this.debugState === 'paused' ? 
            this.renderDebugAction('▶', 'Continue', () => { this.debugState = 'running'; this.sendDebugCommand('continue'); }) :
            this.renderDebugAction('⏸', 'Pause', () => { this.sendDebugCommand('pause'); })
          }
          ${this.renderDebugAction('↷', 'Step Over', () => this.sendDebugCommand('stepOver'))}
          ${this.renderDebugAction('↓', 'Step Into', () => this.sendDebugCommand('stepInto'))}
          ${this.renderDebugAction('↑', 'Step Out', () => this.sendDebugCommand('stepOut'))}
          ${this.renderDebugAction('↻', 'Restart', () => this.sendDebugCommand('restart'))}
          <button class="action-btn stop" title="Stop" @click=${() => { this.debugState = 'inactive'; this.sendDebugCommand('stop'); }}>■</button>
        </div>
      ` : html`
        <div style="padding: 12px; text-align: center; color: #565f89; font-size: 12px;">
          To start debugging, run a launch configuration or attach to a process.
          <br><br>
          <button style="background: #7aa2f7; color: #1a1b26; border: none; padding: 4px 12px; border-radius: 2px; cursor: pointer; font-weight: bold;" @click=${() => { this.debugState = 'running'; this.sendDebugCommand('start', { target: 'extension-host' }); }}>Run and Debug</button>
        </div>
      `}

      <div class="section">
        <div class="section-header ${this.sections.variables ? 'expanded' : ''}" @click=${() => this.toggleSection('variables')}>
          <svg viewBox="0 0 16 16" fill="currentColor"><path d="M6.22 3.22a.75.75 0 0 1 1.06 0l4.25 4.25a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06-1.06L9.94 8 6.22 4.28a.75.75 0 0 1 0-1.06z"/></svg>
          Variables
        </div>
        <div class="section-content ${this.sections.variables ? 'expanded' : ''}">
          ${this.variables.map(v => html`
            <div class="item">
              <span class="var-name">${v.name}:</span>
              <span class="var-value">${v.value}</span>
            </div>
          `)}
        </div>
      </div>

      <div class="section">
        <div class="section-header ${this.sections.watch ? 'expanded' : ''}" @click=${() => this.toggleSection('watch')}>
          <svg viewBox="0 0 16 16" fill="currentColor"><path d="M6.22 3.22a.75.75 0 0 1 1.06 0l4.25 4.25a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06-1.06L9.94 8 6.22 4.28a.75.75 0 0 1 0-1.06z"/></svg>
          Watch
        </div>
        <div class="section-content ${this.sections.watch ? 'expanded' : ''}">
          ${this.watch.map(w => html`
            <div class="item">
              <span class="var-name">${w}:</span>
              <span class="var-value">...</span>
            </div>
          `)}
        </div>
      </div>

      <div class="section">
        <div class="section-header ${this.sections.callStack ? 'expanded' : ''}" @click=${() => this.toggleSection('callStack')}>
          <svg viewBox="0 0 16 16" fill="currentColor"><path d="M6.22 3.22a.75.75 0 0 1 1.06 0l4.25 4.25a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06-1.06L9.94 8 6.22 4.28a.75.75 0 0 1 0-1.06z"/></svg>
          Call Stack
        </div>
        <div class="section-content ${this.sections.callStack ? 'expanded' : ''}">
          ${this.callStack.map(frame => html`
            <div class="item">
              <span class="frame-name">${frame.name}</span>
              <span class="frame-file">${frame.file}:${frame.line}</span>
            </div>
          `)}
        </div>
      </div>

      <div class="section">
        <div class="section-header ${this.sections.breakpoints ? 'expanded' : ''}" @click=${() => this.toggleSection('breakpoints')}>
          <svg viewBox="0 0 16 16" fill="currentColor"><path d="M6.22 3.22a.75.75 0 0 1 1.06 0l4.25 4.25a.75.75 0 0 1 0 1.06l-4.25 4.25a.75.75 0 0 1-1.06-1.06L9.94 8 6.22 4.28a.75.75 0 0 1 0-1.06z"/></svg>
          Breakpoints
        </div>
        <div class="section-content ${this.sections.breakpoints ? 'expanded' : ''}">
          ${this.breakpoints.map(bp => html`
            <div class="item" @click=${() => bp.enabled = !bp.enabled}>
              <div class="checkbox ${bp.enabled ? 'checked' : ''}"></div>
              <span class="bp-file">${bp.file}</span>
              <span class="bp-line">${bp.line}</span>
            </div>
          `)}
        </div>
      </div>
    `;
  }
}
