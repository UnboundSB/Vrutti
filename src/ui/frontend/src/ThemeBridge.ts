/**
 * ThemeBridge
 * 
 * Connects to the Node.js Extension Host to stream Editor theme variables 
 * and convert VS Code tokens into native Vrutti CSS variables.
 */

class ThemeApplier {
  /** Removes every theme variable that is currently applied inline. */
  private static clear(): void {
    const root = document.documentElement;
    const keysToRemove: string[] = [];
    for (let i = 0; i < root.style.length; i++) {
      const key = root.style[i];
      if (key.startsWith('--vrutti-')) {
        keysToRemove.push(key);
      }
    }
    for (const key of keysToRemove) {
      root.style.removeProperty(key);
    }
    document.body.style.backgroundColor = '';
  }

  public static apply(themeData: any): void {
    if (!themeData || !themeData.colors) {
      ThemeApplier.reset();
      return;
    }
    const root = document.documentElement;
    const colors = themeData.colors;

    // Drop the previous theme first. A variable that the new theme does not
    // define must not survive, otherwise colours leak from one theme into
    // the next.
    ThemeApplier.clear();

    for (const [key, value] of Object.entries(colors)) {
      if (key.startsWith('--vrutti-')) {
        root.style.setProperty(key, value as string);
      }
    }

    if (colors['--vrutti-bg']) {
      document.body.style.backgroundColor = colors['--vrutti-bg'];
    }

    // Persist applied theme in localStorage
    try {
      localStorage.setItem('vrutti-theme-colors', JSON.stringify(colors));
      // Re-apply to all Lit elements
      const elements = document.querySelectorAll('*');
      elements.forEach(el => {
        if ((el as any).requestUpdate) {
          (el as any).requestUpdate();
        }
      });

      localStorage.setItem('vrutti-applied-theme', JSON.stringify(themeData));
      window.dispatchEvent(new CustomEvent('theme-loaded', { detail: { isDark: themeData.uiTheme === 'vs-dark' } }));
    } catch (e) {
      console.error('Failed to persist theme state', e);
    }

    console.log(`[ThemeBridge] Applied native Vrutti theme: ${themeData.name || 'Unknown'}`);
  }

  /**
   * Drops the applied theme completely and falls back to the built-in
   * colours. Used when the selected theme cannot be resolved.
   */
  public static reset(): void {
    ThemeApplier.clear();
    try {
      localStorage.removeItem('vrutti-theme-colors');
      localStorage.removeItem('vrutti-applied-theme');
    } catch (e) {
      console.warn('[ThemeBridge] Could not clear the persisted theme state.', e);
    }
    window.dispatchEvent(new CustomEvent('theme-loaded', { detail: { isDark: true } }));
    console.log('[ThemeBridge] Reset to the built-in theme.');
  }
  
  public static loadStartupTheme(): void {
    try {
      const storedColors = localStorage.getItem('vrutti-theme-colors');
      if (storedColors) {
        const colors = JSON.parse(storedColors);
        const root = document.documentElement;
        for (const [key, value] of Object.entries(colors)) {
          if (key.startsWith('--vrutti-')) {
            root.style.setProperty(key, value as string);
          }
        }
        if (colors['--vrutti-bg']) {
          document.body.style.backgroundColor = colors['--vrutti-bg'];
        }
        console.log('[ThemeBridge] Restored startup theme');
      }
    } catch (e) {
      console.error('Failed to load startup theme', e);
    }
  }
}

export class ThemeBridge {
  constructor() {
  }

  public connect(): void {
    console.log('[ThemeBridge] Connecting to Extension Host via IPC...');
    ThemeApplier.loadStartupTheme();
    
    try {
      const stored = localStorage.getItem('vrutti-icon-theme-data');
      if (stored) {
        const theme = JSON.parse(stored);
        window.dispatchEvent(new CustomEvent('vrutti-icon-theme-loaded', { detail: theme }));
      }
    } catch (e) {}

    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.addEventListener('setting-changed', this.handleSettingChanged as EventListener);
    
    // Request installed extensions/themes immediately to trigger component injections
    if ((window as any).sendIpcMessage) {
      (window as any).sendIpcMessage('extensions/request_installed', '{}');
    }
  }

  public disconnect(): void {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    window.removeEventListener('setting-changed', this.handleSettingChanged as EventListener);
  }

  private handleSettingChanged = (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && detail.key === 'workbench.colorTheme') {
      if ((window as any).sendIpcMessage) {
        (window as any).sendIpcMessage('theme/set', JSON.stringify({ name: detail.value }));
      }
    }
    if (detail && detail.key === 'workbench.iconTheme') {
      if ((window as any).sendIpcMessage) {
        (window as any).sendIpcMessage('icon_theme/set', JSON.stringify({ name: detail.value }));
      }
    }
  };

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'theme/load') {
      // A null payload means "fall back to the built-in colours".
      if (msg.params) {
        ThemeApplier.apply(msg.params);
      } else {
        ThemeApplier.reset();
      }
    }
    if (msg && msg.method === 'icon_theme/load') {
      if (msg.params) {
        try {
          localStorage.setItem('vrutti-icon-theme-data', JSON.stringify(msg.params));
        } catch (e) {
          console.warn('[ThemeBridge] Could not save icon theme to localStorage (likely QuotaExceededError).', e);
        }
        window.dispatchEvent(new CustomEvent('vrutti-icon-theme-loaded', { detail: msg.params }));
      } else {
        localStorage.removeItem('vrutti-icon-theme-data');
        window.dispatchEvent(new CustomEvent('vrutti-icon-theme-loaded', { detail: null }));
      }
    }
  };
}

// Singleton export
export const themeBridge = new ThemeBridge();

