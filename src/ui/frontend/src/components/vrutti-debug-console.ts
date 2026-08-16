import { LitElement, css, html } from 'lit';
import { customElement, state, query } from 'lit/decorators.js';

interface DebugLog {
  id: string;
  type: 'log' | 'info' | 'warning' | 'error' | 'evaluation' | 'result';
  text: string;
  isExpanded?: boolean;
}

@customElement('vrutti-debug-console')
export class VruttiDebugConsole extends LitElement {
  @state()
  private logs: DebugLog[] = [
    { id: '1', type: 'info', text: '[Debug] Debug Console initialized. Ready for connections.' }
  ];

  @state() private history: string[] = [];
  @state() private historyIndex = -1;
  @state() private inputValue = '';
  
  // Filtering
  @state() private filterText = '';
  @state() private filterLevel: 'All' | 'Info' | 'Warning' | 'Error' = 'All';

  // Virtualization
  @state() private _scrollTop = 0;
  @state() private containerHeight = 300;
  private itemHeight = 22; // approx height of one line
  
  @query('.debug-log-container')
  private scrollContainer!: HTMLElement;

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('resize', this.handleResize);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('resize', this.handleResize);
  }

  protected firstUpdated() {
    this.handleResize();
    const observer = new ResizeObserver(() => this.handleResize());
    if (this.scrollContainer) observer.observe(this.scrollContainer);
  }

  private handleResize = () => {
    if (this.scrollContainer) {
      this.containerHeight = this.scrollContainer.clientHeight;
    }
  };

  private handleScroll = (e: Event) => {
    this._scrollTop = (e.target as HTMLElement).scrollTop;
  };

  private handleIpc = (e: CustomEvent) => {
    const data = e.detail;
    if (data.method === 'debug/log') {
      this.logs = [...this.logs, {
        id: Math.random().toString(36).substring(7),
        type: data.params.type || 'info',
        text: data.params.text
      }];
      this.scrollToBottom();
    }
  };

  private scrollToBottom() {
    setTimeout(() => {
      if (this.scrollContainer) {
        this.scrollContainer.scrollTop = this.scrollContainer.scrollHeight;
      }
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
        
        if ((window as any).vruttiIpcAsync) {
          (window as any).vruttiIpcAsync(JSON.stringify({
            method: 'debug/evaluate',
            params: { expression: val }
          }));
        }

        this.inputValue = '';
        this.scrollToBottom();
      }
    } else if (e.key === 'ArrowUp' && e.ctrlKey) {
      if (this.historyIndex > 0) {
        this.historyIndex--;
        this.inputValue = this.history[this.historyIndex];
      }
      e.preventDefault();
    } else if (e.key === 'ArrowDown' && e.ctrlKey) {
      if (this.historyIndex < this.history.length - 1) {
        this.historyIndex++;
        this.inputValue = this.history[this.historyIndex];
      } else {
        this.historyIndex = this.history.length;
        this.inputValue = '';
      }
      e.preventDefault();
    }
  }

  private handleInput(e: Event) {
    this.inputValue = (e.target as HTMLTextAreaElement).value;
    
    // Auto-resize textarea
    const target = e.target as HTMLTextAreaElement;
    target.style.height = 'auto';
    target.style.height = Math.min(target.scrollHeight, 150) + 'px';
  }

  public clearConsole = () => {
    this.logs = [];
  };

  private syntaxHighlight(text: string) {
    // Basic formatting for keywords, strings, numbers
    let htmlStr = text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
      
    // Very naive regex highlighting just for wow factor
    htmlStr = htmlStr.replace(/('[^']*'|"[^"]*")/g, '<span class="string">$1</span>');
    htmlStr = htmlStr.replace(/\b(\d+)\b/g, '<span class="number">$1</span>');
    htmlStr = htmlStr.replace(/\b(true|false|null|undefined)\b/g, '<span class="keyword">$1</span>');
    
    return htmlStr;
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

    /* Toolbar */
    .debug-toolbar {
      display: flex;
      align-items: center;
      padding: 4px 8px;
      background: #1f2335;
      border-bottom: 1px solid #292e42;
      gap: 8px;
    }

    .filter-input {
      background: #1a1b26;
      border: 1px solid #3b4261;
      color: #a9b1d6;
      padding: 4px 8px;
      border-radius: 4px;
      font-family: inherit;
      font-size: 12px;
      flex: 1;
      max-width: 250px;
    }
    
    .filter-input:focus {
      outline: 1px solid #7aa2f7;
      border-color: transparent;
    }

    .filter-select {
      background: #1a1b26;
      border: 1px solid #3b4261;
      color: #a9b1d6;
      padding: 4px;
      border-radius: 4px;
      font-size: 12px;
      outline: none;
    }

    .actions {
      display: flex;
      gap: 4px;
      margin-left: auto;
    }

    .actions button {
      background: none;
      border: none;
      color: #717cb4;
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
    }

    .actions button:hover {
      background: #292e42;
      color: #c0caf5;
    }

    /* Logs Area */
    .debug-log-container {
      flex: 1;
      overflow-y: auto;
      position: relative;
      background: #1a1b26;
    }
    
    .virtual-scroll-spacer {
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      width: 100%;
    }

    .virtual-scroll-content {
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      padding: 4px 12px;
    }

    .debug-line {
      min-height: 22px;
      line-height: 22px;
      font-size: 13px;
      white-space: pre-wrap;
      word-break: break-all;
      display: flex;
      align-items: flex-start;
      border-bottom: 1px solid transparent;
    }
    
    .debug-line:hover {
      background: #1f2335;
    }

    .debug-line.log, .debug-line.info { color: #b1bac4; }
    .debug-line.warning { color: #e0af68; }
    .debug-line.error { color: #f7768e; background: rgba(247, 118, 142, 0.1); }
    .debug-line.evaluation { color: #7aa2f7; font-weight: bold; }
    .debug-line.evaluation::before { content: '>'; margin-right: 8px; color: #7aa2f7; }
    .debug-line.result { color: #9ece6a; }
    .debug-line.result::before { content: '<'; margin-right: 8px; color: #9ece6a; }
    
    /* Syntax Highlighting */
    .string { color: #9ece6a; }
    .number { color: #ff9e64; }
    .keyword { color: #bb9af7; italic; }

    /* Input Area */
    .debug-input-container {
      display: flex;
      border-top: 1px solid #292e42;
      padding: 8px;
      align-items: flex-start;
      background: #1f2335;
    }

    .prompt-indicator {
      color: #7aa2f7;
      margin-right: 8px;
      font-weight: bold;
      margin-top: 2px;
    }

    .debug-input {
      flex: 1;
      background: transparent;
      border: none;
      color: #c0caf5;
      font-family: inherit;
      outline: none;
      font-size: 13px;
      resize: none;
      min-height: 20px;
      max-height: 150px;
      line-height: 20px;
    }
    
    .debug-input::placeholder {
      color: #565f89;
    }
  `;

  render() {
    const filteredLogs = this.logs.filter(log => {
      if (this.filterLevel !== 'All') {
        if (this.filterLevel === 'Error' && log.type !== 'error') return false;
        if (this.filterLevel === 'Warning' && log.type !== 'warning' && log.type !== 'error') return false;
        if (this.filterLevel === 'Info' && log.type !== 'info' && log.type !== 'log' && log.type !== 'warning' && log.type !== 'error') return false;
      }
      if (this.filterText && !log.text.toLowerCase().includes(this.filterText.toLowerCase())) return false;
      return true;
    });

    // Virtualization logic
    const totalItems = filteredLogs.length;
    const totalHeight = totalItems * this.itemHeight;
    const startIndex = Math.max(0, Math.floor(this._scrollTop / this.itemHeight) - 5);
    const endIndex = Math.min(totalItems, startIndex + Math.ceil(this.containerHeight / this.itemHeight) + 10);
    const visibleLogs = filteredLogs.slice(startIndex, endIndex);
    const offsetY = startIndex * this.itemHeight;

    return html`
      <div class="debug-toolbar">
        <input 
          class="filter-input" 
          placeholder="Filter logs (e.g. text, object)"
          .value=${this.filterText}
          @input=${(e: Event) => this.filterText = (e.target as HTMLInputElement).value}
        />
        <select class="filter-select" @change=${(e: Event) => this.filterLevel = (e.target as HTMLSelectElement).value as any}>
          <option value="All">All Levels</option>
          <option value="Info">Info</option>
          <option value="Warning">Warning</option>
          <option value="Error">Error</option>
        </select>
        <div class="actions">
          <button title="Clear Console" @click=${this.clearConsole}>
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor">
              <path d="M2.5 1A1.5 1.5 0 0 0 1 2.5v11A1.5 1.5 0 0 0 2.5 15h11a1.5 1.5 0 0 0 1.5-1.5v-11A1.5 1.5 0 0 0 13.5 1h-11zm.5 13V2h10v12H3z"/>
              <path d="M4 4h8v2H4zm0 3h8v2H4zm0 3h5v2H4z"/>
            </svg>
          </button>
        </div>
      </div>
      
      <div class="debug-log-container" @scroll=${this.handleScroll}>
        <div class="virtual-scroll-spacer" style="height: ${totalHeight}px;"></div>
        <div class="virtual-scroll-content" style="transform: translateY(${offsetY}px);">
          ${visibleLogs.map(log => html`
            <div class="debug-line ${log.type}">
              <div style="flex:1" .innerHTML=${this.syntaxHighlight(log.text)}></div>
            </div>
          `)}
        </div>
      </div>
      
      <div class="debug-input-container">
        <span class="prompt-indicator">&gt;</span>
        <textarea 
          class="debug-input" 
          placeholder="Evaluate expression (Enter to submit, Shift+Enter for new line)" 
          .value=${this.inputValue}
          @input=${this.handleInput}
          @keydown=${this.handleInputKeydown}
        ></textarea>
      </div>
    `;
  }
}
