/**
 * ThemeBridge
 * 
 * Connects to the Node.js Extension Host to stream Editor theme variables 
 * and convert VS Code tokens into native Vrutti CSS variables.
 */

class ThemeApplier {
  public static apply(themeData: any): void {
    if (!themeData || !themeData.colors) return;
    const root = document.documentElement;
    const colors = themeData.colors;

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
      // Save full theme data to restore selected dropdown state if needed
      localStorage.setItem('vrutti-applied-theme', JSON.stringify({
        id: themeData.id || themeData.name,
        name: themeData.name,
        uiTheme: themeData.uiTheme
      }));
    } catch (e) {
      console.error('Failed to persist theme state', e);
    }

    console.log(`[ThemeBridge] Applied native Vrutti theme: ${themeData.name || 'Unknown'}`);
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
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  public disconnect(): void {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'theme/load' && msg.params) {
      ThemeApplier.apply(msg.params);
    }
  };
}

// Singleton export
export const themeBridge = new ThemeBridge();

