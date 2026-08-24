import { LitElement, html, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { registry } from '../core/Registry';

@customElement('vrutti-output-view')
export class VruttiOutputView extends LitElement {
  @state() private outputLogs: Record<string, string[]> = {};
  @state() private activeOutputChannel = 'System';
  @state() private outputChannels: string[] = [];

  connectedCallback() {
    super.connectedCallback();
    this.updateFromRegistry();
    registry.addEventListener('change', this.handleRegistryChange);
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    registry.removeEventListener('change', this.handleRegistryChange);
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  private handleRegistryChange = (e: Event) => {
    const type = (e as CustomEvent).detail;
    if (type === 'panel') {
      this.updateFromRegistry();
    }
  };

  private updateFromRegistry() {
    this.outputChannels = registry.getOutputChannels();
    if (!this.outputChannels.includes(this.activeOutputChannel) && this.outputChannels.length > 0) {
      this.activeOutputChannel = this.outputChannels[0];
    }
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'output/log' && msg.params) {
      const channel = msg.params.channel || 'System';
      if (!this.outputLogs[channel]) this.outputLogs[channel] = [];
      this.outputLogs[channel].push(msg.params.text);
      this.requestUpdate();
    }
  };

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      width: 100%;
      height: 100%;
      overflow: hidden;
    }
    
    .output-header {
      display: flex;
      padding: 4px 8px;
      border-bottom: 1px solid var(--vrutti-surface-border, #1f2335);
      background: var(--vrutti-surface, #1a1b26);
    }

    .output-channel-selector select {
      background: var(--vrutti-bg, #0f111a);
      color: var(--vrutti-text-bright, #a9b1d6);
      border: 1px solid var(--vrutti-surface-border, #1f2335);
      padding: 2px 6px;
      border-radius: 4px;
      font-size: 11px;
      outline: none;
      cursor: pointer;
    }

    .output-body {
      flex: 1;
      display: flex;
      overflow: hidden;
      min-width: 0;
      background: var(--vrutti-bg, #0f111a);
    }

    .output-log-container {
      flex: 1;
      overflow-y: auto;
      padding: 8px;
      font-family: 'Fira Code', monospace;
      font-size: 12px;
      color: var(--vrutti-text, #a9b1d6);
    }
    .output-line {
      line-height: 1.4;
      white-space: pre-wrap;
      word-break: break-all;
    }
    
    ::-webkit-scrollbar {
      width: 10px;
      height: 10px;
      background: transparent;
    }
    ::-webkit-scrollbar-thumb {
      background: transparent;
      border-radius: 5px;
    }
    :hover::-webkit-scrollbar-thumb {
      background-color: rgba(122, 162, 247, 0.4);
    }
  `;

  render() {
    return html`
      <div class="output-header">
        <div class="output-channel-selector">
          <select @change=${(e: Event) => this.activeOutputChannel = (e.target as HTMLSelectElement).value}>
            ${this.outputChannels.map(c => html`
              <option value="${c}" ?selected=${this.activeOutputChannel === c}>${c}</option>
            `)}
          </select>
        </div>
      </div>
      <div class="output-body">
        <div class="output-log-container">
          ${(this.outputLogs[this.activeOutputChannel] || []).map(log => html`
            <div class="output-line">${log}</div>
          `)}
        </div>
      </div>
    `;
  }
}
