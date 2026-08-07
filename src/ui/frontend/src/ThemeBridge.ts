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
  constructor() {
  }

  public connect(): void {
    console.log('[ThemeBridge] Connecting to Extension Host via IPC...');
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  public disconnect(): void {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'theme/load' && msg.params) {
      this.applyTheme(msg.params);
    }
  };

  /**
   * Maps Editor standard theme colors to Vrutti custom variables.
   * e.g. "editor.background" -> "--vrutti-bg"
   */
  private applyTheme(themeData: any): void {
    const root = document.documentElement;
    const themeColors = themeData.colors || {};

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

    console.log('[ThemeBridge] Theme applied successfully:', themeData.name || 'Unknown');
  }
}

// Singleton export
export const themeBridge = new ThemeBridge();

