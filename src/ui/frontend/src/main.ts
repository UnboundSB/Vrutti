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
        <div class="logo">
          <img src="../../../../logos/icon.ico" alt="Vrutti" style="height: 20px; vertical-align: middle;" />
        </div>
        <!-- Placeholder for Search -->
        <div>Search files...</div>
        <div>X</div>
      </header>
      <div class="main">
        <vrutti-sidebar></vrutti-sidebar>
        <vrutti-editor></vrutti-editor>
      </div>
      <vrutti-statusbar></vrutti-statusbar>
    `;
  }
}

@customElement('vrutti-sidebar')
export class VruttiSidebar extends LitElement {
  static styles = css`
    :host {
      display: block;
      height: 100%;
    }
    .header {
      padding: 10px 16px;
      font-size: 11px;
      font-weight: 600;
      color: #717cb4;
    }
    .item {
      padding: 4px 16px;
      font-size: 13px;
      cursor: pointer;
    }
    .item:hover {
      background-color: #1e2132;
    }
  `;

  render() {
    return html`
      <div class="header">EXPLORER</div>
      <div class="item">📁 src</div>
      <div class="item">📄 main.cpp</div>
    `;
  }
}

@customElement('vrutti-editor')
export class VruttiEditor extends LitElement {
  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      height: 100%;
      position: relative;
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
      color: #a6accd;
      border-right: 1px solid #23273b;
      border-top: 2px solid #82aaff;
      background-color: #0f111a;
    }
    .content {
      padding: 24px;
      font-family: 'Fira Code', monospace;
      font-size: 14px;
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
      <div class="tabs">
        <div class="tab">main.cpp</div>
      </div>
      <div class="content">
        // Vrutti Core Native Editor<br/>
        #include &lt;iostream&gt;
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
      padding: 0 12px;
      font-size: 12px;
      font-weight: 500;
    }
  `;

  render() {
    return html`
      <div>main* | 0 errors</div>
    `;
  }
}
