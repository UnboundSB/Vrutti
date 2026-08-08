/**
 * ThemeBridge
 * 
 * Connects to the Node.js Extension Host to stream Editor theme variables 
 * and convert VS Code tokens into native Vrutti CSS variables.
 */

class ThemeConverter {
  private static tokenMap: Record<string, string> = {
    'editor.background': '--vrutti-bg',
    'sideBar.background': '--vrutti-surface',
    'activityBar.background': '--vrutti-surface',
    'editorGroupHeader.tabsBackground': '--vrutti-surface',
    'editor.foreground': '--vrutti-text-bright',
    'sideBarTitle.foreground': '--vrutti-text',
    'tab.activeBackground': '--vrutti-surface-border',
    'button.background': '--vrutti-accent',
    'focusBorder': '--vrutti-accent',
    'editorLineNumber.foreground': '--vrutti-text',
    'terminal.background': '--vrutti-bg',
    'gitDecoration.modifiedResourceForeground': '--vrutti-git-modified',
    'gitDecoration.untrackedResourceForeground': '--vrutti-git-untracked',
    'gitDecoration.deletedResourceForeground': '--vrutti-git-deleted'
  };

  public static apply(themeData: any): void {
    if (!themeData || !themeData.colors) return;
    const root = document.documentElement;
    const colors = themeData.colors;

    for (const [vsToken, vruttiVar] of Object.entries(this.tokenMap)) {
      if (colors[vsToken]) {
        root.style.setProperty(vruttiVar, colors[vsToken]);
      }
    }

    if (colors['editor.background']) {
      document.body.style.backgroundColor = colors['editor.background'];
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

    console.log(`[ThemeBridge] Converted and applied theme: ${themeData.name || 'Unknown'}`);
  }
  
  public static loadStartupTheme(): void {
    try {
      const storedColors = localStorage.getItem('vrutti-theme-colors');
      if (storedColors) {
        const colors = JSON.parse(storedColors);
        const root = document.documentElement;
        for (const [vsToken, vruttiVar] of Object.entries(this.tokenMap)) {
          if (colors[vsToken]) {
            root.style.setProperty(vruttiVar, colors[vsToken]);
          }
        }
        if (colors['editor.background']) {
          document.body.style.backgroundColor = colors['editor.background'];
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
    ThemeConverter.loadStartupTheme();
    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  public disconnect(): void {
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'theme/load' && msg.params) {
      ThemeConverter.apply(msg.params);
    }
  };
}

// Singleton export
export const themeBridge = new ThemeBridge();

