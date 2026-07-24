import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './components/vrutti-sidebar';
import './components/vrutti-statusbar';
import './components/vrutti-menubar';

import { globalHoverStyle } from './shared-styles';

@customElement('vrutti-app')
export class VruttiApp extends LitElement {
  @state()
  private isLoading = true;

  @state()
  private greeting = '';

  @state()
  private userName = 'User';

  @state()
  private showSettings = false;

  async connectedCallback() {
    super.connectedCallback();
    
    // Load user configuration
    const storedName = localStorage.getItem('vrutti-username');
    if (storedName) {
      this.userName = storedName;
    }

    const hour = new Date().getHours();
    if (hour >= 5 && hour < 12) this.greeting = 'Good Morning';
    else if (hour >= 12 && hour < 17) this.greeting = 'Good Afternoon';
    else if (hour >= 17 && hour < 22) this.greeting = 'Good Evening';
    else this.greeting = 'Good Night';

    try {
      if ((window as any).vruttiGetSettings) {
        const settings = await (window as any).vruttiGetSettings();
        if (settings && settings['editor.fontFamily']) {
          this.style.setProperty('--vrutti-font', settings['editor.fontFamily']);
        }
        // Fire event to notify settings component if it's already loaded
        window.dispatchEvent(new CustomEvent('settings-loaded', { detail: settings }));
      }
    } catch (e) {
      console.error('Failed to load settings via IPC:', e);
    }

    setTimeout(() => {
      this.isLoading = false;
    }, 2500);

    this.addEventListener('menu-action', this.handleMenuAction);
    this.addEventListener('close-settings', this.handleCloseSettings);
    this.addEventListener('setting-changed', this.handleSettingChanged);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('menu-action', this.handleMenuAction);
    this.removeEventListener('close-settings', this.handleCloseSettings);
    this.removeEventListener('setting-changed', this.handleSettingChanged);
  }

  private handleMenuAction = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail.action === 'Preferences') {
      if (!customElements.get('vrutti-settings')) {
        await import('./components/vrutti-settings');
      }
      this.showSettings = true;
    }
  };

  private handleCloseSettings = () => {
    this.showSettings = false;
  };

  private handleSettingChanged = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    console.log('[Main] Routing setting save to IPC:', detail.key, detail.value);
    
    if (detail.key === 'editor.fontFamily') {
      this.style.setProperty('--vrutti-font', detail.value);
    }

    if ((window as any).vruttiUpdateSetting) {
      await (window as any).vruttiUpdateSetting(detail.key, detail.value);
    }
  };

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      flex-direction: column;
      width: 100vw;
      height: 100vh;
      overflow: hidden;
      --vrutti-bg: hsl(230, 25%, 8%);
      --vrutti-surface: hsla(230, 25%, 15%, 0.6);
      --vrutti-surface-border: hsla(230, 25%, 25%, 0.4);
      --vrutti-accent: hsl(210, 100%, 75%);
      --vrutti-text: hsl(220, 25%, 75%);
      --vrutti-text-bright: hsl(220, 25%, 95%);
      background-color: var(--vrutti-bg);
      color: var(--vrutti-text);
      font-family: 'Inter', -apple-system, sans-serif;
    }
    
    header {
      height: 35px;
      background: var(--vrutti-surface);
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
      display: flex;
      align-items: center;
      justify-content: space-between;
      border-bottom: 1px solid var(--vrutti-surface-border);
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
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
    }
    .header-right {
      display: flex;
      height: 100%;
    }
    .actions button {
      background: none;
      border: none;
      color: var(--vrutti-text);
      width: 46px;
      height: 100%;
      display: flex;
      align-items: center;
      justify-content: center;
      -webkit-app-region: no-drag;
      cursor: default;
      padding: 0;
      transition: background 0.2s ease, color 0.2s ease;
    }
    .actions button:hover {
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
    }
    .actions button.close-btn:hover {
      background-color: #e81123;
      color: white;
    }
    
    .logo {
      font-weight: 600;
      color: #82aaff;
    }

    .header-center {
      display: flex;
      flex: 1;
      justify-content: center;
      align-items: center;
      -webkit-app-region: drag;
    }

    .command-center {
      display: flex;
      align-items: center;
      background: var(--vrutti-bg);
      border: 1px solid var(--vrutti-surface-border);
      border-radius: 6px;
      padding: 2px 12px;
      width: 400px;
      height: 24px;
      -webkit-app-region: no-drag;
      color: var(--vrutti-text);
      font-size: 12px;
      cursor: text;
      transition: border-color 0.2s, background 0.2s;
    }

    .command-center:hover {
      background: var(--vrutti-surface);
      border-color: var(--vrutti-accent);
    }

    .command-center svg {
      margin-right: 8px;
      opacity: 0.7;
    }

    .layout-controls {
      display: flex;
      align-items: center;
      margin-right: 8px;
      -webkit-app-region: no-drag;
    }

    .layout-controls button {
      width: 28px;
      height: 24px;
      border-radius: 4px;
      margin: 0 2px;
    }

    .main {
      display: flex;
      flex: 1;
      overflow: hidden;
      position: relative;
    }
    
    vrutti-sidebar {
      background: var(--vrutti-surface);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border-right: 1px solid var(--vrutti-surface-border);
      flex-shrink: 0;
    }
    
    .editor-container {
      display: flex;
      flex-direction: column;
      flex: 1;
      height: 100%;
      position: relative;
    }
    
    vrutti-editor {
      flex: 1;
      background-color: transparent;
      min-height: 0;
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
      width: 40vw;
      max-width: 512px;
      object-fit: contain;
    }
    
    .splash-screen {
      position: absolute;
      top: 35px;
      left: 0;
      right: 0;
      bottom: 0;
      background: var(--vrutti-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      z-index: 1000;
      transition: opacity 0.6s cubic-bezier(0.4, 0, 0.2, 1), visibility 0.6s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .splash-screen.hidden {
      opacity: 0;
      visibility: hidden;
      pointer-events: none;
    }
    .greeting {
      font-size: 2rem;
      font-weight: 300;
      color: #a6accd;
      margin-bottom: 8px;
    }
    .sub-greeting {
      font-size: 1.1rem;
      color: #717cb4;
      font-style: italic;
    }
    .splash-logo {
      width: 48px;
      height: 48px;
      margin-bottom: 24px;
      animation: pulse 2s infinite ease-in-out;
      object-fit: contain;
    }
    @keyframes pulse {
      0% { transform: scale(0.95); opacity: 0.7; }
      50% { transform: scale(1.05); opacity: 1; }
      100% { transform: scale(0.95); opacity: 0.7; }
    }
  ];

  render() {
    return html`
      <header @mousedown="${(e: MouseEvent) => {
        const target = e.target as HTMLElement;
        if (target.tagName !== 'BUTTON' && !target.closest('button') && !target.classList.contains('menu-item') && !target.closest('.command-center')) {
          if ((window as any).startWindowDrag) {
            (window as any).startWindowDrag();
          }
        }
      }}">
        <div class="header-left">
          <img class="logo-img" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAABL1BMVEVHcEwxNzknKTUxJzAjKT1xT1o8MDxFOUIgKUMlL0pVOUYlLURkMD0eJjwxM0M2Mjk8OUQgKUN1XWYuMkA0LEJrSlYiJzghKD9uW2QdJTs7NESRWWMoLT4tNEqLUl01NkcrNE01NksoK0JAPElbQU1nN0eLQE9gPUpkPkxqTlpLOUc1OlAnKTBSJjQwMkQdIzWBRlOEWGFVLjsmLUA9LT8aITYXYm6KOEhTO0luO0olKDggKkaZS1YcJT2QWWKOUl6JXGaBTlpoQU9QP0tPPU0lLkuPRFE9M0EzNElTNEI/LkChVGCsTlmhTltzPk19Q1E1O1GjPU+tQlKpSVZVNkVMNUZCL0FvTVlbQFB0RFFCRldOUl9dM0ZaRE8nL0Y6QVdQO0wrNE5ILkBITVwfKkYMoBU4AAAAZHRSTlMAAhIX8zMbIKHHOL/LhYEMI5QWb+k9Kd0bzE9qquWXjvCVr2Wttq3hhCuk8ZFWylnWgsH8vUsEbth6eZDqtK63Vd7OQ/Sa1bPz6fzdtHDEoTISQyXH89mnlca4RxW2/MDL1WpWS6ieYwAAAL9JREFUGNNNj0MSBAEUQ38bY9u2bds273+GqVF1Z5WXRSoB+EharxHAk8Q5dlfkHFMOLzEbVbV/xjqrk48yD8q/BLGs737nzj0fmtNvRhUOFliPywcLg+Hdk8dvl4f/6t2oJu1iDAFK01LTgYD+uPW42EyUALVOY5varQdy2WvkhEIVIFiqxDDnva3fzMZDGPKppZ9GU9euTJL/ISRj0VnFSj03VUQXTMYwygURgSyBi3nnUIUMD0r5d0EkkHzNCy8jF6c8+rZRAAAAAElFTkSuQmCC" style="display: block; width: 16px; height: 16px; object-fit: contain;" />
          <vrutti-menubar></vrutti-menubar>
        </div>

        <div class="header-center">
          <div class="command-center">
            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M10.68 11.74a6 6 0 0 1-7.922-8.982 6 6 0 0 1 8.982 7.922l3.04 3.04a.749.749 0 0 1-.326 1.275.749.749 0 0 1-.734-.215ZM11.5 7a4.499 4.499 0 1 0-8.997 0A4.499 4.499 0 0 0 11.5 7Z"></path></svg>
            <span>Vrutti IDE - Search</span>
          </div>
        </div>
        
        <div class="header-right actions">
          <div class="layout-controls">
            <button title="Toggle Sidebar">
              <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M2 3a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3zm11 0H7v10h6V3zM6 3H3v10h3V3z"></path></svg>
            </button>
            <button title="Toggle Panel">
              <svg viewBox="0 0 16 16" width="16" height="16" fill="currentColor"><path d="M2 3a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3zm11 9H3v2h10v-2zM3 11h10V3H3v8z"></path></svg>
            </button>
          </div>
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
      
      <div class="splash-screen ${this.isLoading ? '' : 'hidden'}">
        <img class="splash-logo" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAMAAAAoLQ9TAAABL1BMVEVHcEwxNzknKTUxJzAjKT1xT1o8MDxFOUIgKUMlL0pVOUYlLURkMD0eJjwxM0M2Mjk8OUQgKUN1XWYuMkA0LEJrSlYiJzghKD9uW2QdJTs7NESRWWMoLT4tNEqLUl01NkcrNE01NksoK0JAPElbQU1nN0eLQE9gPUpkPkxqTlpLOUc1OlAnKTBSJjQwMkQdIzWBRlOEWGFVLjsmLUA9LT8aITYXYm6KOEhTO0luO0olKDggKkaZS1YcJT2QWWKOUl6JXGaBTlpoQU9QP0tPPU0lLkuPRFE9M0EzNElTNEI/LkChVGCsTlmhTltzPk19Q1E1O1GjPU+tQlKpSVZVNkVMNUZCL0FvTVlbQFB0RFFCRldOUl9dM0ZaRE8nL0Y6QVdQO0wrNE5ILkBITVwfKkYMoBU4AAAAZHRSTlMAAhIX8zMbIKHHOL/LhYEMI5QWb+k9Kd0bzE9qquWXjvCVr2Wttq3hhCuk8ZFWylnWgsH8vUsEbth6eZDqtK63Vd7OQ/Sa1bPz6fzdtHDEoTISQyXH89mnlca4RxW2/MDL1WpWS6ieYwAAAL9JREFUGNNNj0MSBAEUQ38bY9u2bds273+GqVF1Z5WXRSoB+EharxHAk8Q5dlfkHFMOLzEbVbV/xjqrk48yD8q/BLGs737nzj0fmtNvRhUOFliPywcLg+Hdk8dvl4f/6t2oJu1iDAFK01LTgYD+uPW42EyUALVOY5varQdy2WvkhEIVIFiqxDDnva3fzMZDGPKppZ9GU9euTJL/ISRj0VnFSj03VUQXTMYwygURgSyBi3nnUIUMD0r5d0EkkHzNCy8jF6c8+rZRAAAAAElFTkSuQmCC" />
        <div class="greeting">${this.greeting}, ${this.userName}</div>
        <div class="sub-greeting">prepping your beast...</div>
      </div>
      
      <div class="main">
        <vrutti-sidebar></vrutti-sidebar>
        <div class="editor-container">
          ${this.showSettings ? html`
            <vrutti-settings style="position: absolute; top: 0; left: 0; right: 0; bottom: 0;"></vrutti-settings>
          ` : html`
            <vrutti-editor></vrutti-editor>
            <img src="../../../../logos/logo-512x512.png" class="underlay" />
          `}
        </div>
      </div>
      <vrutti-statusbar></vrutti-statusbar>
    `;
  }
}


