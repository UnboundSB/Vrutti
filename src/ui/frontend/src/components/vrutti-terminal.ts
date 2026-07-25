import { LitElement, css, html } from 'lit';
import { customElement, query } from 'lit/decorators.js';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import { xtermStyles } from './xterm-styles';

@customElement('vrutti-terminal')
export class VruttiTerminal extends LitElement {
  private terminal!: Terminal;
  private fitAddon!: FitAddon;

  @query('#terminal-container')
  private container!: HTMLElement;

  static styles = [xtermStyles, css`
    :host {
      display: flex;
      width: 100%;
      height: 100%;
      background: rgba(13, 17, 23, 0.8);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      flex-direction: column;
      border-top: 1px solid rgba(255, 255, 255, 0.08);
      box-shadow: 0 -4px 24px rgba(0, 0, 0, 0.4);
    }
    
    .panel-tabs {
      height: 35px;
      background: transparent;
      display: flex;
      align-items: flex-end;
      padding: 0 16px;
      font-size: 11px;
      text-transform: uppercase;
      font-weight: 500;
      letter-spacing: 0.5px;
      user-select: none;
    }
    
    .tab {
      padding: 8px 12px;
      color: rgba(255, 255, 255, 0.5);
      cursor: pointer;
      border-bottom: 1px solid transparent;
      transition: color 0.1s, border-color 0.1s;
    }
    
    .tab:hover {
      color: rgba(255, 255, 255, 0.8);
    }
    
    .tab.active {
      color: #82aaff;
      border-bottom: 1px solid #82aaff;
    }

    .panel-actions {
      margin-left: auto;
      display: flex;
      align-items: center;
      padding-bottom: 8px;
    }
    
    .panel-actions button {
      background: none;
      border: none;
      color: rgba(255, 255, 255, 0.5);
      cursor: pointer;
      padding: 0 4px;
    }
    
    .panel-actions button:hover {
      color: #fff;
    }

    #terminal-container {
      flex: 1;
      padding: 8px;
      overflow: hidden;
      min-height: 0;
    }

    /* Customize xterm scrollbar for better appearance */
    .xterm-viewport::-webkit-scrollbar {
      width: 10px;
    }
    .xterm-viewport::-webkit-scrollbar-track {
      background: transparent;
    }
    .xterm-viewport::-webkit-scrollbar-thumb {
      background: rgba(255, 255, 255, 0.1);
      border-radius: 5px;
    }
    .xterm-viewport::-webkit-scrollbar-thumb:hover {
      background: rgba(255, 255, 255, 0.2);
    }
  `];

  async connectedCallback() {
    super.connectedCallback();
    
    // Initialize terminal on next tick to ensure DOM is ready
    setTimeout(() => this.initTerminal(), 0);
    
    window.addEventListener('resize', this.handleResize);
    
    // Bind global output callback for C++ backend
    (window as any).vruttiTerminalOutput = (b64data: string) => {
      if (this.terminal) {
        const binString = atob(b64data);
        const bytes = new Uint8Array(binString.length);
        for (let i = 0; i < binString.length; i++) {
          bytes[i] = binString.charCodeAt(i);
        }
        this.terminal.write(bytes);
      }
    };
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('resize', this.handleResize);
    if (this.terminal) {
      this.terminal.dispose();
    }
    (window as any).vruttiTerminalOutput = undefined;
  }

  private initTerminal() {
    this.terminal = new Terminal({
      theme: {
        background: 'transparent',
        foreground: '#e4f0fb',
        cursor: '#58a6ff',
        selectionBackground: 'rgba(88, 166, 255, 0.3)',
        black: '#484f58',
        red: '#ff7b72',
        green: '#3fb950',
        yellow: '#d29922',
        blue: '#58a6ff',
        magenta: '#bc8cff',
        cyan: '#39c5cf',
        white: '#b1bac4',
        brightBlack: '#6e7681',
        brightRed: '#ffa198',
        brightGreen: '#56d364',
        brightYellow: '#e3b341',
        brightBlue: '#79c0ff',
        brightMagenta: '#d2a8ff',
        brightCyan: '#56d4dd',
        brightWhite: '#ffffff'
      },
      fontFamily: "'Fira Code', 'Cascadia Code', Consolas, monospace",
      fontSize: 13,
      fontWeight: '500',
      cursorBlink: true,
      allowTransparency: true
    });

    this.fitAddon = new FitAddon();
    this.terminal.loadAddon(this.fitAddon);
    
    this.terminal.open(this.container);
    this.fitAddon.fit();

    // Send input to backend
    this.terminal.onData((data) => {
      if ((window as any).vruttiTerminalInput) {
        try {
          (window as any).vruttiTerminalInput(btoa(data));
        } catch (e) {
          console.error('Failed to send terminal input', e);
        }
      }
    });

    // Notify backend of initial size
    this.reportSize();

    // Init backend terminal
    if ((window as any).vruttiTerminalInit) {
      (window as any).vruttiTerminalInit().catch(console.error);
    }
  }

  private handleResize = () => {
    if (this.fitAddon) {
      this.fitAddon.fit();
      this.reportSize();
    }
  };

  private async reportSize() {
    if (this.terminal && (window as any).vruttiTerminalResize) {
      try {
        await (window as any).vruttiTerminalResize(this.terminal.cols, this.terminal.rows);
      } catch (e) {
        console.error('Failed to report terminal size', e);
      }
    }
  }

  render() {
    return html`
      <div class="panel-tabs">
        <div class="tab">Problems</div>
        <div class="tab">Output</div>
        <div class="tab">Debug Console</div>
        <div class="tab active">Terminal</div>
        <div class="tab">Ports</div>
        <div class="panel-actions">
          <button title="Clear" @click=${() => this.terminal.clear()}>
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2.5 1A1.5 1.5 0 0 0 1 2.5v11A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-11A1.5 1.5 0 0 0 13.5 1h-11zm4.854 4.146a.5.5 0 0 1 0 .708L5.707 7.5H11.5a.5.5 0 0 1 0 1H5.707l1.647 1.646a.5.5 0 0 1-.708.708l-2.5-2.5a.5.5 0 0 1 0-.708l2.5-2.5a.5.5 0 0 1 .708 0z"/></svg>
          </button>
        </div>
      </div>
      <div id="terminal-container"></div>
    `;
  }
}
