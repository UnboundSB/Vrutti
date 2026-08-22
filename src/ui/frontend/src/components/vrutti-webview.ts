import { LitElement, css, html } from 'lit';
import { customElement, property, state, query } from 'lit/decorators.js';

@customElement('vrutti-webview')
export class VruttiWebview extends LitElement {
  @property({ type: String })
  webviewId: string = '';

  @property({ type: String })
  viewId: string = '';

  @query('iframe')
  private iframe!: HTMLIFrameElement;

  private _html: string = '';

  static styles = css`
    :host {
      display: block;
      width: 100%;
      height: 100%;
      background-color: var(--vrutti-bg, #1e1e1e);
      color: var(--vrutti-text, #cccccc);
    }
    iframe {
      width: 100%;
      height: 100%;
      border: none;
      background-color: transparent;
    }
  `;

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('message', this.handleIframeMessage as EventListener);

    if (this.viewId) {
      if (!this.webviewId) {
        this.webviewId = `webview_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      }
      if ((window as any).sendIpcMessage) {
        (window as any).sendIpcMessage('webviewView/resolve', JSON.stringify({ viewId: this.viewId, webviewId: this.webviewId }));
      }
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('message', this.handleIframeMessage as EventListener);
  }

  private handleIpc = (e: CustomEvent) => {
    const msg = e.detail;
    if (msg.method === 'webview/setHtml' && msg.params && msg.params.id === this.webviewId) {
      this._html = msg.params.html;
      if (this.iframe) {
        this.iframe.srcdoc = this._html;
      }
    } else if (msg.method === 'webview/postMessage' && msg.params && msg.params.id === this.webviewId) {
      if (this.iframe && this.iframe.contentWindow) {
        this.iframe.contentWindow.postMessage(msg.params.message, '*');
      }
    }
  };

  private handleIframeMessage = (e: MessageEvent) => {
    // If the message is coming from our iframe
    if (e.source === this.iframe?.contentWindow) {
      if ((window as any).sendIpcMessage) {
        (window as any).sendIpcMessage('webview/receiveMessage', JSON.stringify({
          id: this.webviewId,
          message: e.data
        }));
      }
    }
  };

  render() {
    return html`
      <iframe 
        sandbox="allow-scripts allow-forms allow-same-origin allow-downloads" 
        srcdoc=${this._html}
      ></iframe>
    `;
  }
}
