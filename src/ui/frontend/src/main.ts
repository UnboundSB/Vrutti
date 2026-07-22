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
      border-bottom: 1px solid #23273b;
      -webkit-app-region: drag;
      user-select: none;
    }
    
    .header-left {
      display: flex;
      align-items: center;
      padding-left: 10px;
    }
    .logo-img {
      width: 16px;
      height: 16px;
      margin-right: 12px;
    }
    .menu-item {
      padding: 4px 8px;
      font-size: 13px;
      color: #a6accd;
      -webkit-app-region: no-drag;
      cursor: default;
      border-radius: 4px;
    }
    .menu-item:hover {
      background-color: #23273b;
    }
    .header-right {
      display: flex;
      height: 100%;
    }
    .actions button {
      background: none;
      border: none;
      color: #a6accd;
      width: 46px;
      height: 100%;
      display: flex;
      align-items: center;
      justify-content: center;
      -webkit-app-region: no-drag;
      cursor: default;
      padding: 0;
    }
    .actions button:hover {
      background-color: #23273b;
    }
    .actions button.close-btn:hover {
      background-color: #e81123;
      color: white;
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
        <div class="header-left">
          <img src="../../../../logos/logo-16x16.png" class="logo-img" />
          <div class="menu-item">File</div>
          <div class="menu-item">Edit</div>
          <div class="menu-item">Selection</div>
          <div class="menu-item">View</div>
        </div>
        <div class="header-right actions">
          <button @click="${() => (window as any).minimizeWindow()}">
            <svg width="11" height="11" viewBox="0 0 11 11" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M11 4.399V5.5H0V4.399h11z" fill="currentColor"/></svg>
          </button>
          <button @click="${() => (window as any).maximizeWindow()}">
            <svg width="11" height="11" viewBox="0 0 11 11" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M11 0v11H0V0h11zM9.899 1.1H1.1v8.8h8.799V1.1z" fill="currentColor"/></svg>
          </button>
          <button class="close-btn" @click="${() => (window as any).closeWindow()}">
            <svg width="11" height="11" viewBox="0 0 11 11" fill="none" xmlns="http://www.w3.org/2000/svg"><path d="M5.5 4.793l4.146-4.147.708.708L6.207 5.5l4.147 4.146-.708.708L5.5 6.207l-4.146 4.147-.708-.708L4.793 5.5.646 1.354l.708-.708L5.5 4.793z" fill="currentColor"/></svg>
          </button>
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
      display: flex;
      flex-direction: column;
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
    .tree {
      flex: 1;
      overflow-y: auto;
    }
    .item {
      padding: 4px 16px;
      font-size: 13px;
      color: #a6accd;
      cursor: pointer;
      display: flex;
      align-items: center;
      user-select: none;
    }
    .item:hover {
      background-color: #1e2132;
    }
    .item .icon {
      margin-right: 6px;
      font-size: 14px;
    }
  `;

  render() {
    return html`
      <div class="header">WORKSPACE EXPLORER</div>
      <div style="padding: 16px; color: #a6accd; opacity: 0.5; font-size: 13px;">
        Awaiting FS Sync via IPC...
      </div>
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
    .tab {
      padding: 0 16px;
      display: flex;
      align-items: center;
      font-size: 13px;
      color: #717cb4;
      border-right: 1px solid #23273b;
      cursor: pointer;
    }
    .tab.active {
      color: #a6accd;
      border-top: 2px solid #82aaff;
      background-color: #0f111a;
    }
    .content {
      display: flex;
      flex: 1;
      font-family: 'Fira Code', 'Consolas', monospace;
      font-size: 14px;
      color: #a6accd;
      z-index: 1;
      overflow-y: auto;
      padding: 12px 0;
    }
    .gutter {
      padding: 0 16px 0 24px;
      color: #4b526d;
      text-align: right;
      user-select: none;
    }
    .code {
      flex: 1;
      white-space: pre;
    }
    .code .keyword { color: #c792ea; }
    .code .string { color: #c3e88d; }
    .code .function { color: #82aaff; }
    .code .comment { color: #717cb4; font-style: italic; }

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
    .section {
      display: flex;
      align-items: center;
    }
    .item {
      margin: 0 8px;
      cursor: pointer;
    }
    .item:hover {
      opacity: 0.8;
    }
  `;

  render() {
    return html`
      <div class="section">
        <span class="item">ᚠ main</span>
        <span class="item">⊗ 0  ⚠ 0</span>
      </div>
      <div class="section">
        <span class="item">Ln 14, Col 28</span>
        <span class="item">UTF-8</span>
        <span class="item">CRLF</span>
        <span class="item">C++</span>
      </div>
    `;
  }
}
