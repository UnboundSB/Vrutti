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
      <header @mousedown="${(e: MouseEvent) => {
        const target = e.target as HTMLElement;
        if (target.tagName !== 'BUTTON' && !target.closest('button') && !target.classList.contains('menu-item')) {
          if ((window as any).startWindowDrag) {
            (window as any).startWindowDrag();
          }
        }
      }}">
        <div class="header-left">
          <img class="logo-img" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAABL1BMVEVHcEwxNzknKTUxJzAjKT1xT1o8MDxFOUIgKUMlL0pVOUYlLURkMD0eJjwxM0M2Mjk8OUQgKUN1XWYuMkA0LEJrSlYiJzghKD9uW2QdJTs7NESRWWMoLT4tNEqLUl01NkcrNE01NksoK0JAPElbQU1nN0eLQE9gPUpkPkxqTlpLOUc1OlAnKTBSJjQwMkQdIzWBRlOEWGFVLjsmLUA9LT8aITYXYm6KOEhTO0luO0olKDggKkaZS1YcJT2QWWKOUl6JXGaBTlpoQU9QP0tPPU0lLkuPRFE9M0EzNElTNEI/LkChVGCsTlmhTltzPk19Q1E1O1GjPU+tQlKpSVZVNkVMNUZCL0FvTVlbQFB0RFFCRldOUl9dM0ZaRE8nL0Y6QVdQO0wrNE5ILkBITVwfKkYMoBU4AAAAZHRSTlMAAhIX8zMbIKHHOL/LhYEMI5QWb+k9Kd0bzE9qquWXjvCVr2Wttq3hhCuk8ZFWylnWgsH8vUsEbth6eZDqtK63Vd7OQ/Sa1bPz6fzdtHDEoTISQyXH89mnlca4RxW2/MDL1WpWS6ieYwAAAL9JREFUGNNNj0MSBAEUQ38bY9u2bds273+GqVF1Z5WXRSoB+EharxHAk8Q5dlfkHFMOLzEbVbV/xjqrk48yD8q/BLGs737nzj0fmtNvRhUOFliPywcLg+Hdk8dvl4f/6t2oJu1iDAFK01LTgYD+uPW42EyUALVOY5varQdy2WvkhEIVIFiqxDDnva3fzMZDGPKppZ9GU9euTJL/ISRj0VnFSj03VUQXTMYwygURgSyBi3nnUIUMD0r5d0EkkHzNCy8jF6c8+rZRAAAAAElFTkSuQmCC" style="display: block; width: 16px; height: 16px; object-fit: contain;" />
          <div class="menu-item">File</div>
          <div class="menu-item">Edit</div>
          <div class="menu-item">Selection</div>
          <div class="menu-item">View</div>
        </div>
        
        <div class="header-right actions">
          <button @click=${() => (window as any).minimizeWindow()}>
            <svg viewBox="0 0 10 1" width="10" height="1"><rect width="10" height="1" fill="currentColor"/></svg>
          </button>
          <button @click=${() => (window as any).maximizeWindow()}>
            <svg viewBox="0 0 10 10" width="10" height="10" fill="none" stroke="currentColor"><rect x="0.5" y="0.5" width="9" height="9"/></svg>
          </button>
          <button class="close-btn" @click=${() => (window as any).closeWindow()}>
            <svg viewBox="0 0 10 10" width="10" height="10" stroke="currentColor"><path d="M0,0 L10,10 M10,0 L0,10" stroke-width="1.5"/></svg>
          </button>
        </div>
      </header>
      
      <div style="flex: 1; display: flex; align-items: center; justify-content: center; opacity: 0.1; pointer-events: none;">
        <h1 style="font-size: 6rem; letter-spacing: 0.5rem; font-family: sans-serif;">Vrutti</h1>
      </div>
    `;
  }
}
