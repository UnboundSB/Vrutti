/**
 * ThemeBridge
 * 
 * This module is responsible for connecting to the Node.js Extension Host 
 * to stream Editor theme variables into our custom Lit frontend.
 * 
 * Vrutti IDE maps standard Editor token colors into custom glassmorphism
 * and modern variables.
 */

export class ThemeBridge {
  private extensionHostUrl: string;
  private ws: WebSocket | null = null;

  constructor(extensionHostUrl: string = 'ws://localhost:9090') {
    this.extensionHostUrl = extensionHostUrl;
  }

  public connect(): void {
    this.ws = new WebSocket(this.extensionHostUrl);

    this.ws.onopen = () => {
      console.log('[ThemeBridge] Connected to Extension Host');
      this.requestActiveTheme();
    };

    this.ws.onmessage = (event) => {
      try {
        const payload = JSON.parse(event.data);
        if (payload.type === 'THEME_UPDATE') {
          this.applyTheme(payload.theme);
        }
      } catch (err) {
        console.error('[ThemeBridge] Failed to parse theme payload:', err);
      }
    };

    this.ws.onerror = (err) => {
      console.error('[ThemeBridge] WebSocket error:', err);
    };

    this.ws.onclose = () => {
      console.log('[ThemeBridge] Disconnected. Reconnecting in 5s...');
      setTimeout(() => this.connect(), 5000);
    };
  }

  private requestActiveTheme(): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ type: 'GET_ACTIVE_THEME' }));
    }
  }

  /**
   * Maps Editor standard theme colors to Vrutti custom variables.
   * e.g. "editor.background" -> "--vrutti-bg"
   */
  private applyTheme(themeColors: Record<string, string>): void {
    const root = document.documentElement;

    // Example mapping - this will be expanded as we integrate more of the Ext Host
    if (themeColors['editor.background']) {
      // Vrutti overrides plain colors with glassmorphism, so we might convert 
      // the hex to HSL and adjust, or just apply it directly to a base layer.
      root.style.setProperty('--vrutti-bg-raw', themeColors['editor.background']);
    }

    if (themeColors['activityBar.background']) {
      root.style.setProperty('--vrutti-surface-raw', themeColors['activityBar.background']);
    }

    if (themeColors['editor.foreground']) {
      root.style.setProperty('--vrutti-text', themeColors['editor.foreground']);
    }

    console.log('[ThemeBridge] Theme applied successfully.');
  }
}

// Singleton export
export const themeBridge = new ThemeBridge();
