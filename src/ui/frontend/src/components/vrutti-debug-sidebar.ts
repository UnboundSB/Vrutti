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

interface DebuggerInfo {
  type: string;
  label: string;
  extensionName: string;
}

@customElement('vrutti-debug-sidebar')
export class VruttiDebugSidebar extends LitElement {
  @state() private debugState: 'inactive' | 'paused' | 'running' = 'inactive';
  @state() private availableDebuggers: DebuggerInfo[] = [];
  @state() private selectedDebuggerType: string = '';
  @state() private activeFile: string = '';
  @state() private activeThreadId: number = 1;
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

  async connectedCallback() {
    super.connectedCallback();
    this.activeFile = (window as any).currentActiveFile || '';

    if ((window as any).vruttiBreakpoints) {
      const allBp: Breakpoint[] = [];
      for (const [file, lines] of Object.entries((window as any).vruttiBreakpoints)) {
        for (const line of lines as number[]) {
          allBp.push({ file, line, enabled: true });
        }
      }
      this.breakpoints = allBp;
    }

    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('vrutti-breakpoints-changed', this.handleBreakpointsChanged as EventListener);
    window.addEventListener('active-file-changed', this.handleActiveFileChanged as EventListener);
    
    setTimeout(() => {
      if ((window as any).sendIpcMessage) {
        (window as any).sendIpcMessage('debuggers/available', '{}');
      }
    }, 500);
  }

  disconnectedCallback() {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('vrutti-breakpoints-changed', this.handleBreakpointsChanged as EventListener);
    window.removeEventListener('active-file-changed', this.handleActiveFileChanged as EventListener);
    super.disconnectedCallback();
  }

  private formatLocalPath(path: string): string {
    if (!path) return '';
    if (path.startsWith('file://')) {
      let decoded = decodeURIComponent(path);
      decoded = decoded.replace('file:///', '').replace('file://', ''); 
      if (/^[a-zA-Z]:/.test(decoded)) {
        decoded = decoded.replace(/\//g, '\\');
      } else {
        if (!decoded.startsWith('/')) decoded = '/' + decoded;
      }
      return decoded;
    }
    return path;
  }
  
  private getBasename(path: string): string {
    return path.split(/[\\/]/).pop() || path;
  }

  private handleActiveFileChanged = (e: CustomEvent) => {
    if (e.detail && e.detail.path) {
      this.activeFile = e.detail.path;
    }
  };

  private handleBreakpointsChanged = (e: CustomEvent) => {
    const detail = e.detail;
    if (detail && detail.file && Array.isArray(detail.lines)) {
      const newBreakpoints = this.breakpoints.filter(b => b.file !== detail.file);
      for (const line of detail.lines) {
        newBreakpoints.push({ file: detail.file, line: line, enabled: true });
      }
      this.breakpoints = newBreakpoints;

      if (this.debugState !== 'inactive') {
        this.sendDapCommand('setBreakpoints', {
          source: { path: this.formatLocalPath(detail.file) },
          breakpoints: detail.lines.map((l: number) => ({ line: l }))
        });
      }
    }
  };

  private handleIpc = (e: any) => {
    const data = e.detail;
    if (data.method === 'debuggers/available') {
      this.availableDebuggers = data.params || [];
      if (this.availableDebuggers.length > 0 && !this.selectedDebuggerType) {
        this.selectedDebuggerType = this.availableDebuggers[0].type;
      }
    } else if (data.method === 'dap/event') {
      const msg = data.params;
      if (msg.event === 'initialized') {
        const fileToLines = new Map<string, number[]>();
        for (const bp of this.breakpoints) {
          const formattedPath = this.formatLocalPath(bp.file);
          if (!fileToLines.has(formattedPath)) fileToLines.set(formattedPath, []);
          fileToLines.get(formattedPath)!.push(bp.line);
        }
        
        for (const [file, lines] of fileToLines.entries()) {
          this.sendDapCommand('setBreakpoints', {
            source: { path: file },
            breakpoints: lines.map((l: number) => ({ line: l }))
          });
        }
        
        this.sendDapCommand('configurationDone');
      } else if (msg.event === 'stopped') {
        this.debugState = 'paused';
        
        if ((window as any).sendIpcMessage) {
          (window as any).sendIpcMessage('dap/request', JSON.stringify({ command: 'threads', args: {} }));
        }
      } else if (msg.event === 'continued') {
        this.debugState = 'running';
        this.callStack = [];
        this.variables = [];
        window.dispatchEvent(new CustomEvent('vrutti-debug-resume'));
      } else if (msg.event === 'terminated' || msg.event === 'exited') {
        this.debugState = 'inactive';
        this.callStack = [];
        this.variables = [];
        window.dispatchEvent(new CustomEvent('vrutti-debug-resume'));
      } else if (msg.event === 'vrutti-dap-started') {
        this.sendDapCommand('initialize', {
            clientID: 'vrutti',
            clientName: 'Vrutti IDE',
            adapterID: this.selectedDebuggerType,
            pathFormat: 'path',
            linesStartAt1: true,
            columnsStartAt1: true,
            supportsVariableType: true,
            supportsVariablePaging: true,
            supportsRunInTerminalRequest: true,
            locale: 'en'
        });
      }
    } else if (data.method === 'dap/response') {
      const resp = data.params;
      if (resp.command === 'initialize' && resp.success) {
        this.sendDapCommand('launch', {
          program: this.formatLocalPath(this.activeFile),
          cwd: this.formatLocalPath((window as any).vruttiWorkspaceDir || ''),
          stopOnEntry: false
        });
      } else if (resp.command === 'threads' && resp.success) {
        const threads = resp.body.threads;
        if (threads && threads.length > 0) {
          this.activeThreadId = threads[0].id;
          if ((window as any).sendIpcMessage) {
            (window as any).sendIpcMessage('dap/request', JSON.stringify({ command: 'stackTrace', args: { threadId: this.activeThreadId } }));
          }
        }
      } else if (resp.command === 'stackTrace' && resp.success) {
        const stackFrames = resp.body.stackFrames || [];
        this.callStack = stackFrames.map((f: any) => ({
          id: f.id,
          name: f.name,
          file: f.source ? f.source.name : '(unknown)',
          line: f.line
        }));
        
        if (this.callStack.length > 0) {
          const topFrame = this.callStack[0];
          window.dispatchEvent(new CustomEvent('vrutti-debug-pause', {
            detail: { file: topFrame.file, line: topFrame.line }
          }));
          
          if ((window as any).sendIpcMessage) {
            (window as any).sendIpcMessage('dap/request', JSON.stringify({ command: 'scopes', args: { frameId: topFrame.id } }));
          }
        }
      } else if (resp.command === 'scopes' && resp.success) {
        const scopes = resp.body.scopes || [];
        if (scopes.length > 0) {
          if ((window as any).sendIpcMessage) {
            (window as any).sendIpcMessage('dap/request', JSON.stringify({ command: 'variables', args: { variablesReference: scopes[0].variablesReference } }));
          }
        }
      } else if (resp.command === 'variables' && resp.success) {
        const variables = resp.body.variables || [];
        this.variables = variables.map((v: any) => ({
          name: v.name,
          value: v.value,
          type: v.type || '',
          hasChildren: v.variablesReference > 0
        }));
      }
    }
  };

  public sendDapCommand(command: string, args: any = {}) {
    let cwd = '';
    const active = (window as any).currentActiveFile;
    if (active) {
        const formatted = this.formatLocalPath(active);
        cwd = formatted.substring(0, formatted.lastIndexOf('/'));
        if (!cwd && formatted.lastIndexOf('\\') !== -1) {
            cwd = formatted.substring(0, formatted.lastIndexOf('\\'));
        }
    }
    
    if (!(window as any).sendIpcMessage) return;

    if (command === 'start') {
        (window as any).sendIpcMessage('dap/start', JSON.stringify({ type: this.selectedDebuggerType, cwd }));
    } else if (command === 'stop') {
        (window as any).sendIpcMessage('dap/stop', '{}');
    } else {
        (window as any).sendIpcMessage('dap/request', JSON.stringify({
            command,
            args: { ...args, cwd }
        }));
    }
  }

  private toggleSection(section: keyof typeof this.sections) {
    this.sections = { ...this.sections, [section]: !this.sections[section] };
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
      flex-wrap: wrap;
      padding: 8px;
      gap: 4px;
      background: var(--vrutti-surface-border, #1f2335);
      border-bottom: 1px solid #292e42;
      justify-content: center;
    }

    .dap-btn {
      background: none;
      border: none;
      color: #7aa2f7;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .dap-btn[disabled] {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .dap-btn:not([disabled]):hover { background: #292e42; }

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

    .breakpoint-item { display: flex; align-items: center; width: 100%; }
    .breakpoint-icon { width: 8px; height: 8px; border-radius: 50%; background: #f7768e; margin-right: 8px; }
    .breakpoint-file { color: #a9b1d6; margin-right: 8px; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
    .breakpoint-line { color: #ff9e64; }
  `;

  render() {
    return html`
      ${this.debugState !== 'inactive' ? html`
        <div class="debug-toolbar">
          ${this.debugState === 'paused' ? html`
            <button title="Continue" class="dap-btn" @click=${() => { this.debugState = 'running'; this.sendDapCommand('continue', { threadId: this.activeThreadId }); }}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M4 2v12l10-6L4 2z"/></svg></button>
          ` : html`
            <button title="Pause" class="dap-btn" @click=${() => this.sendDapCommand('pause', { threadId: this.activeThreadId })}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M5 2h2v12H5V2zm4 0h2v12H9V2z"/></svg></button>
          `}
          <button title="Step Over" class="dap-btn" ?disabled=${this.debugState !== 'paused'} @click=${() => this.sendDapCommand('next', { threadId: this.activeThreadId })}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M10.5 3h3v10h-3v-1.5h1.5v-7h-1.5V3zM2 8l4-4v3h5v2H6v3L2 8z"/></svg></button>
          <button title="Step Into" class="dap-btn" ?disabled=${this.debugState !== 'paused'} @click=${() => this.sendDapCommand('stepIn', { threadId: this.activeThreadId })}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M8 2l4 4H9v6H7V6H4l4-4zM2 13h12v2H2v-2z"/></svg></button>
          <button title="Step Out" class="dap-btn" ?disabled=${this.debugState !== 'paused'} @click=${() => this.sendDapCommand('stepOut', { threadId: this.activeThreadId })}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M8 14l-4-4h3V4h2v6h3l-4 4zM2 1h12v2H2V1z"/></svg></button>
          <button title="Restart" class="dap-btn" style="color: #9ece6a;" @click=${() => this.sendDapCommand('restart')}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M8 2a6 6 0 1 0 6 6h-2a4 4 0 1 1-4-4v2l4-3-4-3v2z"/></svg></button>
          <button title="Stop" class="dap-btn" style="color: #f7768e;" @click=${() => { this.debugState = 'inactive'; this.sendDapCommand('stop'); }}><svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M4 4h8v8H4V4z"/></svg></button>
        </div>
      ` : html`
        <div style="padding: 12px; text-align: center; color: #565f89; font-size: 12px; box-sizing: border-box; width: 100%;">
          To start debugging, select a debugger and run.
          <br><br>
          <select style="display: block; box-sizing: border-box; background: #1a1b26; color: #a9b1d6; border: 1px solid #3b4261; padding: 4px; border-radius: 2px; width: 100%; margin-bottom: 8px;"
            @change=${(e: any) => this.selectedDebuggerType = e.target.value}
            .value=${this.selectedDebuggerType}>
            ${this.availableDebuggers.map(d => html`<option value="${d.type}">${d.label} (${d.extensionName})</option>`)}
            ${this.availableDebuggers.length === 0 ? html`<option value="">No debuggers installed</option>` : ''}
          </select>
          <button style="display: block; box-sizing: border-box; background: #7aa2f7; color: #1a1b26; border: none; padding: 4px 12px; border-radius: 2px; cursor: pointer; font-weight: bold; width: 100%;" 
            ?disabled=${!this.selectedDebuggerType}
            @click=${() => { 
                this.debugState = 'running'; 
                this.sendDapCommand('start'); 
                window.dispatchEvent(new CustomEvent('menu-action', { detail: { action: 'New Terminal' }, bubbles: true, composed: true }));
                window.dispatchEvent(new CustomEvent('switch-to-debug-console'));
            }}>Run and Debug</button>
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
              <div class="breakpoint-item">
                <div class="breakpoint-icon"></div>
                <span class="breakpoint-file" title="${this.formatLocalPath(bp.file)}">${this.getBasename(this.formatLocalPath(bp.file))}</span>
                <span class="breakpoint-line">${bp.line}</span>
              </div>
            </div>
          `)}
        </div>
      </div>
    `;
  }
}
