import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface DebugLog {
  id: string;
  type: 'log' | 'error' | 'evaluation' | 'result';
  text: string;
}

@customElement('vrutti-debug-console')
export class VruttiDebugConsole extends LitElement {
  @state()
  private logs: DebugLog[] = [
    { id: '1', type: 'log', text: '[Debug] Debug Console initialized.' }
  ];

  @state()
  private history: string[] = [];
  
  @state()
  private historyIndex = -1;

  @state()
  private inputValue = '';

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-debug-log', this.handleDebugLog as EventListener);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-debug-log', this.handleDebugLog as EventListener);
  }

  private handleDebugLog = (e: CustomEvent<{ type: DebugLog['type'], text: string }>) => {
    const detail = e.detail;
    if (detail && detail.text) {
      this.logs = [...this.logs, {
        id: Math.random().toString(36).substring(7),
        type: detail.type || 'log',
        text: detail.text
      }];
      this.scrollToBottom();
    }
  };

  private scrollToBottom() {
    setTimeout(() => {
      const container = this.shadowRoot?.querySelector('.debug-log-container');
      if (container) container.scrollTop = container.scrollHeight;
    }, 10);
  }

  private handleInputKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      const val = this.inputValue.trim();
      if (val) {
        if (this.history[this.history.length - 1] !== val) {
          this.history.push(val);
        }
        this.historyIndex = this.history.length;
        
        this.logs = [...this.logs, {
          id: Math.random().toString(36).substring(7),
          type: 'evaluation',
          text: val
        }];
        
        this.dispatchEvent(new CustomEvent('evaluate-expression', {
          detail: { expression: val },
          bubbles: true,
          composed: true
        }));

        this.inputValue = '';
        this.scrollToBottom();
      }
    } else if (e.key === 'ArrowUp') {
      if (this.historyIndex > 0) {
        this.historyIndex--;
        this.inputValue = this.history[this.historyIndex];
      }
      e.preventDefault();
    } else if (e.key === 'ArrowDown') {
      if (this.historyIndex < this.history.length - 1) {
        this.historyIndex++;
        this.inputValue = this.history[this.historyIndex];
      } else {
        this.historyIndex = this.history.length;
        this.inputValue = '';
      }
    }
  }

  private handleInput(e: Event) {
    this.inputValue = (e.target as HTMLInputElement).value;
  }

  public clearConsole() {
    this.logs = [];
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      flex: 1;
      height: 100%;
      overflow: hidden;
      background: var(--vscode-editor-background, #1a1b26);
      font-family: 'Fira Code', 'Cascadia Code', Consolas, monospace;
    }

    .debug-log-container {
      flex: 1;
      overflow-y: auto;
      padding: 8px 12px;
      font-size: 13px;
      white-space: pre-wrap;
      word-break: break-all;
    }

    .debug-line {
      min-height: 1.2em;
      margin-bottom: 2px;
      display: flex;
    }

    .debug-line.log { color: #b1bac4; }
    .debug-line.error { color: #f7768e; }
    .debug-line.evaluation { color: #7aa2f7; font-weight: bold; }
    .debug-line.evaluation::before {
      content: '>';
      margin-right: 8px;
    }
    .debug-line.result { color: #9ece6a; }
    .debug-line.result::before {
      content: '<';
      margin-right: 8px;
    }

    .debug-input-container {
      display: flex;
      border-top: 1px solid #1f2335;
      padding: 4px 8px;
      align-items: center;
      background: #1a1b26;
    }

    .prompt-indicator {
      color: #7aa2f7;
      margin-right: 8px;
      font-weight: bold;
    }

    .debug-input {
      flex: 1;
      background: transparent;
      border: none;
      color: #a9b1d6;
      font-family: inherit;
      outline: none;
      font-size: 13px;
    }
    
    .actions {
      position: absolute;
      top: 4px;
      right: 14px;
      display: flex;
      gap: 4px;
    }

    .actions button {
      background: none;
      border: none;
      color: #717cb4;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
    }

    .actions button:hover {
      background: #292e42;
      color: #c0caf5;
    }
  `;

  render() {
    return html`
      <div class="actions">
        <button title="Clear Console" @click=${this.clearConsole}>
          <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor">
            <path d="M2.5 1A1.5 1.5 0 0 0 1 2.5v11A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-11A1.5 1.5 0 0 0 13.5 1h-11zm.5 13V2h10v12H3z"/>
            <path d="M4 4h8v2H4zm0 3h8v2H4zm0 3h5v2H4z"/>
          </svg>
        </button>
      </div>
      <div class="debug-log-container">
        ${this.logs.map(log => html`
          <div class="debug-line ${log.type}">${log.text}</div>
        `)}
      </div>
      <div class="debug-input-container">
        <span class="prompt-indicator">&gt;</span>
        <input 
          type="text" 
          class="debug-input" 
          placeholder="Evaluate expression" 
          .value=${this.inputValue}
          @input=${this.handleInput}
          @keydown=${this.handleInputKeydown}
        />
      </div>
    `;
  }
}
