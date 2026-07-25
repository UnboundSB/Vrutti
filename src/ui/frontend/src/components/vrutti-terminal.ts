import { LitElement, css, html } from 'lit';
import { customElement, query } from 'lit/decorators.js';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';

@customElement('vrutti-terminal')
export class VruttiTerminal extends LitElement {
  private terminal!: Terminal;
  private fitAddon!: FitAddon;

  @query('#terminal-container')
  private container!: HTMLElement;

  static styles = css`
    :host {
      display: flex;
      width: 100%;
      height: 100%;
      background: var(--vrutti-bg, #0d1117);
      flex-direction: column;
    }
    
    .terminal-header {
      height: 28px;
      background: var(--vrutti-surface, #161b22);
      border-bottom: 1px solid var(--vrutti-surface-border, #30363d);
      display: flex;
      align-items: center;
      padding: 0 10px;
      font-size: 11px;
      color: var(--vrutti-text, #c9d1d9);
      text-transform: uppercase;
      font-weight: 600;
      letter-spacing: 0.5px;
      user-select: none;
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
  `;

  async connectedCallback() {
    super.connectedCallback();
    
    // Initialize terminal on next tick to ensure DOM is ready
    setTimeout(() => this.initTerminal(), 0);
    
    window.addEventListener('resize', this.handleResize);
    
    // Bind global output callback for C++ backend
    (window as any).vruttiTerminalOutput = (data: string) => {
      if (this.terminal) {
        this.terminal.write(data);
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
        background: '#0d1117',
        foreground: '#c9d1d9',
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
      fontFamily: "Consolas, 'Courier New', monospace",
      fontSize: 14,
      cursorBlink: true,
      allowTransparency: true
    });

    this.fitAddon = new FitAddon();
    this.terminal.loadAddon(this.fitAddon);
    
    this.terminal.open(this.container);
    this.fitAddon.fit();

    // Send input to backend
    this.terminal.onData(async (data) => {
      if ((window as any).vruttiTerminalInput) {
        try {
          await (window as any).vruttiTerminalInput(data);
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
      <div class="terminal-header">Terminal</div>
      <div id="terminal-container"></div>
    `;
  }
}
