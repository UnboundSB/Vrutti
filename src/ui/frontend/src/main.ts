import { LitElement, css, html, TemplateResult } from 'lit';
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

  files = [
    { name: 'src', type: 'folder', open: true, children: [
        { name: 'core', type: 'folder', open: false, children: [] },
        { name: 'ui', type: 'folder', open: true, children: [
            { name: 'Window.cpp', type: 'file' },
            { name: 'Window.h', type: 'file' }
        ]}
    ]},
    { name: 'CMakeLists.txt', type: 'file' },
    { name: 'README.md', type: 'file' }
  ];

  renderTree(nodes: any[], depth = 0): TemplateResult[] {
    return nodes.map(node => html`
      <div class="item" style="padding-left: ${16 + depth * 12}px">
        <span class="icon">${node.type === 'folder' ? (node.open ? '📂' : '📁') : '📄'}</span>
        ${node.name}
      </div>
      ${node.type === 'folder' && node.open && node.children ? this.renderTree(node.children, depth + 1) : ''}
    `);
  }

  render() {
    return html`
      <div class="header">WORKSPACE EXPLORER</div>
      <div class="tree">
        ${this.renderTree(this.files)}
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

  tabs = ['main.cpp', 'Window.cpp', 'IPCClient.h'];
  activeTab = 'Window.cpp';
  
  mockCode = [
    html`<span class="keyword">#include</span> <span class="string">"Window.h"</span>`,
    html`<span class="keyword">#include</span> <span class="string">&lt;iostream&gt;</span>`,
    html``,
    html`<span class="keyword">namespace</span> vrutti::ui {`,
    html`    Window::Window(<span class="keyword">int</span> width, <span class="keyword">int</span> height) {`,
    html`        <span class="comment">// Initialize native webview</span>`,
    html`        std::cout &lt;&lt; <span class="string">"[UI] Initializing Native Webview..."</span> &lt;&lt; std::endl;`,
    html`    }`,
    html`}`
  ];

  render() {
    return html`
      <div class="tabs">
        ${this.tabs.map(t => html`<div class="tab ${t === this.activeTab ? 'active' : ''}">${t}</div>`)}
      </div>
      <div class="content">
        <div class="gutter">
          ${this.mockCode.map((_, i) => html`<div>${i + 1}</div>`)}
        </div>
        <div class="code">
          ${this.mockCode.map(line => html`<div>${line || html`&nbsp;`}</div>`)}
        </div>
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
