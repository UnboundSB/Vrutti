import { LitElement, css, html } from 'lit';
import { customElement, query } from 'lit/decorators.js';
import { Terminal } from 'xterm';
import { FitAddon } from 'xterm-addon-fit';
import 'xterm/css/xterm.css';

@customElement('vrutti-terminal')
export class VruttiTerminal extends LitElement {
  private term!: Terminal;
  private fitAddon!: FitAddon;

  @query('#terminal-container')
  private container!: HTMLElement;

  static styles = css`
    :host {
      display: block;
      width: 100%;
      height: 100%;
      background-color: var(--vrutti-bg, #1e1e1e);
      border-top: 1px solid var(--vrutti-surface-border, #333);
      padding: 4px;
      box-sizing: border-box;
      overflow: hidden;
    }
    
    #terminal-container {
      width: 100%;
      height: 100%;
    }
  `;

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('resize', this.handleResize);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('resize', this.handleResize);
    this.term?.dispose();
  }

  firstUpdated() {
    this.initTerminal();
  }

  private initTerminal() {
    this.term = new Terminal({
      theme: {
        background: '#1e1e1e',
        foreground: '#cccccc',
        cursor: '#ffffff'
      },
      fontFamily: 'Consolas, "Courier New", monospace',
      fontSize: 14,
      cursorBlink: true
    });

    this.fitAddon = new FitAddon();
    this.term.loadAddon(this.fitAddon);

    this.term.open(this.container);
    this.fitAddon.fit();

    this.term.writeln('\x1b[1;32mVrutti IDE Terminal\x1b[0m');
    this.term.writeln('Welcome to the integrated terminal.');
    this.term.write('$ ');

    // Mock input handling for now (until we connect to PTY via IPC)
    this.term.onData(e => {
      switch (e) {
        case '\r': // Enter
          this.term.writeln('');
          this.term.write('$ ');
          break;
        case '\x7F': // Backspace
          this.term.write('\b \b');
          break;
        default:
          this.term.write(e);
      }
    });
  }

  private handleResize = () => {
    if (this.fitAddon) {
      this.fitAddon.fit();
    }
  };

  render() {
    return html`
      <div id="terminal-container"></div>
    `;
  }
}
