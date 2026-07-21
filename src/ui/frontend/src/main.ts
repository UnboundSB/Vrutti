import { LitElement, css, html } from 'lit';
import { customElement } from 'lit/decorators.js';

@customElement('vrutti-app')
export class VruttiApp extends LitElement {
  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100vh;
      background-color: #0f111a;
      color: #a6accd;
      font-family: 'Inter', -apple-system, sans-serif;
    }
    
    header {
      height: 35px;
      background-color: #181b28;
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0 16px;
      border-bottom: 1px solid #23273b;
      -webkit-app-region: drag;
    }
    
    .logo {
      font-weight: 600;
      color: #82aaff;
    }

    .main {
      display: flex;
      flex: 1;
      overflow: hidden;
    }
    
    vrutti-sidebar {
      width: 250px;
      background-color: #13151f;
      border-right: 1px solid #23273b;
    }
    
    vrutti-editor {
      flex: 1;
      background-color: #0f111a;
    }
  `;

  render() {
    return html`
      <header>
        <div class="title">Vrutti IDE Core</div>
        <div class="actions">
          <button style="background: none; border: none; color: #a6accd; cursor: pointer;">_</button>
          <button style="background: none; border: none; color: #a6accd; cursor: pointer;">□</button>
          <button style="background: none; border: none; color: #a6accd; cursor: pointer;">x</button>
        </div>
      </header>
      <div class="main">
        <vrutti-workspace></vrutti-workspace>
        <vrutti-editor-surface></vrutti-editor-surface>
      </div>
      <vrutti-statusbar></vrutti-statusbar>
    `;
  }
}

@customElement('vrutti-workspace')
export class VruttiWorkspace extends LitElement {
  static styles = css`
    :host {
      display: block;
      width: 250px;
      height: 100%;
      background-color: #13151f;
      border-right: 1px solid #23273b;
    }
    .header {
      padding: 10px 16px;
      font-size: 11px;
      font-weight: 600;
      color: #717cb4;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .status {
      padding: 16px;
      font-size: 13px;
      color: #a6accd;
      opacity: 0.7;
    }
  `;

  render() {
    return html`
      <div class="header">WORKSPACE EXPLORER</div>
      <div class="status">Awaiting FS Sync via IPC...</div>
    `;
  }
}

@customElement('vrutti-editor-surface')
export class VruttiEditorSurface extends LitElement {
  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      flex: 1;
      height: 100%;
      position: relative;
      background-color: #0f111a;
    }
    .tabs {
      height: 35px;
      background-color: #13151f;
      display: flex;
      border-bottom: 1px solid #23273b;
      z-index: 1;
    }
    .content {
      display: flex;
      align-items: center;
      justify-content: center;
      flex: 1;
      font-family: 'Fira Code', monospace;
      font-size: 14px;
      color: #717cb4;
      z-index: 1;
    }
    .underlay {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      opacity: 0.05;
      pointer-events: none;
      z-index: 0;
    }
  `;

  render() {
    return html`
      <div class="tabs"></div>
      <div class="content">
        [PieceTable Buffer Empty]
      </div>
      <img src="../../../../logos/logo-512x512.png" class="underlay" />
    `;
  }
}

@customElement('vrutti-statusbar')
export class VruttiStatusbar extends LitElement {
  static styles = css`
    :host {
      display: flex;
      height: 24px;
      background-color: #82aaff;
      color: #0f111a;
      align-items: center;
      justify-content: space-between;
      padding: 0 12px;
      font-size: 12px;
      font-weight: 500;
    }
  `;

  render() {
    return html`
      <div>IPC: Disconnected</div>
      <div>ThreadPool: Active</div>
    `;
  }
}
