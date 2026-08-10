import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './components/vrutti-sidebar';
import './components/vrutti-statusbar';
import './components/vrutti-menubar';
import './components/vrutti-panel';
import './components/vrutti-editor-layout';
import './components/vrutti-quick-open';
import './components/vrutti-task-picker';
import './components/vrutti-extension-details';
import { VruttiTaskManager } from './components/vrutti-task-manager';
import { themeBridge } from './ThemeBridge';

import { globalHoverStyle } from './shared-styles';

@customElement('vrutti-app')
export class VruttiApp extends LitElement {
  @state()
  private isLoading = true;

  @state()
  private contextMenu: { x: number, y: number, path: string, name: string, isDirectory: boolean } | null = null;

  @state() private activeExtension: any = null;

  @state() private showSettings = false;
  @state() private showGitGraph = false;
  @state() private showTaskPicker = false;
  @state() private taskPickerMode: 'run' | 'defaultBuild' | 'active' = 'run';
  @state() private showTerminal = false;
  @state() private showQuickOpen = false;
  @state() private terminalHeight = 300;

  private isResizingTerminal = false;


  private startY = 0;
  private startHeight = 0;

  private startTerminalResize = (e: MouseEvent) => {
    this.isResizingTerminal = true;
    this.startY = e.clientY;
    this.startHeight = this.terminalHeight;
    window.addEventListener('mousemove', this.doTerminalResize);
    window.addEventListener('mouseup', this.stopTerminalResize);
    document.body.style.cursor = 'ns-resize';
  };

  private doTerminalResize = (e: MouseEvent) => {
    if (!this.isResizingTerminal) return;
    const dy = this.startY - e.clientY;
    let newHeight = this.startHeight + dy;
    if (newHeight < 100) newHeight = 100;
    if (newHeight > window.innerHeight - 150) newHeight = window.innerHeight - 150;
    this.terminalHeight = newHeight;
    window.dispatchEvent(new Event('resize'));
  };

  private stopTerminalResize = () => {
    this.isResizingTerminal = false;
    window.removeEventListener('mousemove', this.doTerminalResize);
    window.removeEventListener('mouseup', this.stopTerminalResize);
    document.body.style.cursor = '';
  };

  @state()
  private globalSettings: Record<string, any> = {};



  @state()
  private greeting = '';

  @state()
  private userName = 'User';




  async connectedCallback() {
    super.connectedCallback();
    
    // Expose global output writer
    (window as any).vruttiWriteOutput = (channel: string, text: string) => {
      window.dispatchEvent(new CustomEvent('vrutti-output-write', { detail: { channel, text } }));
    };

    (window as any).vruttiIpcMessage = (b64: string) => {
      try {
        const jsonStr = decodeURIComponent(escape(atob(b64)));
        const msg = JSON.parse(jsonStr);
        
        // Dispatch event so other components can listen to it
        window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));
        
        if (msg.method === 'run/output') {
           if ((window as any).vruttiWriteOutput) {
               (window as any).vruttiWriteOutput('Execution', msg.params.text);
           }
        }
      } catch (e) {
        console.error("Failed to parse IPC message from backend:", e);
      }
    };

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

    // Connect theme bridge to listen for IPC themes
    themeBridge.connect();

    this.addEventListener('menu-action', this.handleMenuAction);
    this.addEventListener('close-settings', this.handleCloseSettings);
    this.addEventListener('setting-changed', this.handleSettingChanged);
    this.addEventListener('open-file', this.handleOpenFile as EventListener);
    this.addEventListener('open-context-menu', this.handleContextMenu as EventListener);
    this.addEventListener('open-git-graph', this.handleOpenGitGraph);
    this.addEventListener('close-git-graph', this.handleCloseGitGraph);
    window.addEventListener('click', this.closeContextMenu);
    window.addEventListener('keydown', this.handleGlobalKeydown);
    this.addEventListener('extension-selected', this.handleExtensionSelected as EventListener);
    this.addEventListener('close-extension-details', this.handleCloseExtensionDetails);
    
    // Fetch initial settings from native
    if ((window as any).vruttiGetSettings) {
      (window as any).vruttiGetSettings().then((jsonStr: string) => {
        try {
          const settings = JSON.parse(jsonStr);
          this.globalSettings = settings;
          
          // Broadcast them to components
          for (const key in settings) {
            window.dispatchEvent(new CustomEvent('setting-changed', {
              detail: { key, value: settings[key] }
            }));
          }
        } catch (e) {
          console.error("Failed to parse settings", e);
        }
      }).catch(console.error);
    }
  }

  private toggleSetting(key: string) {
    const currentVal = this.globalSettings[key];
    const newVal = !currentVal;
    this.globalSettings = { ...this.globalSettings, [key]: newVal };
    
    // Broadcast to UI components (e.g. vrutti-editor)
    window.dispatchEvent(new CustomEvent('setting-changed', {
      detail: { key, value: newVal }
    }));

    // Update native settings manager
    if ((window as any).vruttiUpdateSetting) {
      (window as any).vruttiUpdateSetting(key, newVal);
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('menu-action', this.handleMenuAction);
    this.removeEventListener('close-settings', this.handleCloseSettings);
    this.removeEventListener('setting-changed', this.handleSettingChanged);
    this.removeEventListener('open-file', this.handleOpenFile as EventListener);
    this.removeEventListener('open-context-menu', this.handleContextMenu as EventListener);
    this.removeEventListener('open-git-graph', this.handleOpenGitGraph);
    this.removeEventListener('close-git-graph', this.handleCloseGitGraph);
    window.removeEventListener('click', this.closeContextMenu);
    window.removeEventListener('keydown', this.handleGlobalKeydown);
    this.removeEventListener('extension-selected', this.handleExtensionSelected as EventListener);
    this.removeEventListener('close-extension-details', this.handleCloseExtensionDetails);
  }

  private handleMenuAction = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail.action === 'Preferences') {
      if (!customElements.get('vrutti-settings')) {
        await import('./components/vrutti-settings');
      }
      this.showSettings = true;
    } else if (detail.action === 'Word Wrap') {
      this.toggleSetting('editor.wordWrap');
    } else if (detail.action === 'Auto Save') {
      this.toggleSetting('files.autoSave');
    } else if (detail.action === 'openFolder') {
      if ((window as any).vruttiOpenFolderDialog) {
        try {
          const json = await (window as any).vruttiOpenFolderDialog();
          if (json && json.path && json.path !== "") {
            let openInNewWindow = false;
            if ((window as any).currentWorkspace) {
              openInNewWindow = window.confirm("Do you want to open this folder in a new window?");
            }
            if (openInNewWindow) {
              if ((window as any).vruttiOpenNewWindow) {
                await (window as any).vruttiOpenNewWindow(json.path);
              }
            } else {
              window.dispatchEvent(new CustomEvent('workspace-changed', {
                detail: { path: json.path }
              }));
            }
          }
        } catch (err) {
          console.error("Failed to open folder dialog", err);
        }
      }
    } else if (detail.action === 'toggleTerminal') {
      this.toggleTerminal();
    } else if (['Explorer', 'Search', 'Source Control', 'Run', 'Extensions'].includes(detail.action)) {
      const sidebar = this.shadowRoot?.querySelector('vrutti-sidebar') as any;
      if (sidebar && sidebar.selectTab) {
        const tabMap: Record<string, string> = {
          'Explorer': 'explorer',
          'Search': 'search',
          'Source Control': 'scm',
          'Run': 'debug',
          'Extensions': 'extensions'
        };
        sidebar.selectTab(tabMap[detail.action]);
      }
    } else if (detail.action === 'Toggle Developer Tools') {
      if ((window as any).vruttiToggleDevTools) {
        (window as any).vruttiToggleDevTools();
      }
    } else if (detail.action === 'New File') {
      const layout = this.shadowRoot?.querySelector('#main-layout') as any;
      if (layout && layout.openFile) {
          layout.openFile('Untitled-1');
      }
    } else if (detail.action === 'Open File') {
      if ((window as any).vruttiOpenFileDialog) {
        try {
          const jsonStr = await (window as any).vruttiOpenFileDialog();
          const json = typeof jsonStr === 'string' ? JSON.parse(jsonStr) : jsonStr;
          if (json && json.path) {
            const layout = this.shadowRoot?.querySelector('#main-layout') as any;
            if (layout && layout.openFile) {
                layout.openFile(json.path);
            }
          }
        } catch (err) {
          console.error("Failed to open file dialog", err);
        }
      }
    } else if (detail.action === 'Close Editor') {
      const layout = this.shadowRoot?.querySelector('#main-layout') as any;
      if (layout && layout.closeActiveEditor) {
          layout.closeActiveEditor();
      }
    } else if (['Command Palette', 'Show All Commands', 'Open View', 'Go to File'].includes(detail.action)) {
      this.showQuickOpen = true;
    } else if (detail.action === 'About') {
      window.alert('Vrutti IDE\nVersion 1.0.0\nBuilt by UnboundSB');
    } else if (detail.action === 'Check for Updates') {
      window.alert('There are currently no updates available.');
    } else if (detail.action === 'Appearance' || detail.action === 'Editor Layout') {
      this.showSettings = true;
    } else if (['New Terminal'].includes(detail.action)) {
      this.showTerminal = true;
    } else if (detail.action === 'Run Task') {
      this.taskPickerMode = 'run';
      this.showTaskPicker = true;
    } else if (detail.action === 'Active Tasks') {
      this.taskPickerMode = 'active';
      this.showTaskPicker = true;
    } else if (detail.action === 'Configure Default Build Task') {
      this.taskPickerMode = 'defaultBuild';
      this.showTaskPicker = true;
    } else if (detail.action === 'Configure Tasks') {
      VruttiTaskManager.ensureTasksFileExists().then(path => {
          if (path) {
              window.dispatchEvent(new CustomEvent('open-file', { detail: { path: path, name: 'tasks.json' } }));
          }
      });
    } else if (detail.action === 'Build Task') {
      VruttiTaskManager.runDefaultBuildTask().then(ran => {
          if (!ran) {
              this.taskPickerMode = 'defaultBuild';
              this.showTaskPicker = true;
          }
      });
    } else {
      const editorActions = [
        'Undo', 'Redo', 'Cut', 'Copy', 'Paste', 'Find', 'Replace', 'Select All',
        'Expand Selection', 'Add Cursor Above', 'Add Cursor Below', 'Add Next Occurrence',
        'Select All Occurrences', 'Toggle Line Comment', 'Toggle Block Comment', 
        'Shrink Selection', 'Copy Line Up', 'Copy Line Down', 'Move Line Up', 
        'Move Line Down', 'Duplicate Selection', 'Add Cursors to Line Ends', 
        'Add Previous Occurrence', 'Save',
        // Go Actions
        'Back', 'Forward', 'Go to Symbol in Workspace', 'Go to Line/Column',
        'Go to Definition', 'Go to Declaration', 'Go to Type Definition',
        'Go to Implementations', 'Go to References', 'Next Problem', 'Previous Problem',
        'Next Change', 'Previous Change',
        // Run/Debug Actions
        'Start Debugging', 'Run Without Debugging', 'Stop Debugging', 'Restart Debugging',
        'Step Over', 'Step Into', 'Step Out', 'Continue', 'Toggle Breakpoint'
      ];
      
      const externalLinks: Record<string, string> = {
        'Welcome': 'https://github.com/UnboundSB/Vrutti',
        'Documentation': 'https://github.com/UnboundSB/Vrutti/wiki',
        'Editor Playground': 'https://github.com/UnboundSB/Vrutti/wiki/Playground',
        'Release Notes': 'https://github.com/UnboundSB/Vrutti/releases',
        'Keyboard Shortcuts Reference': 'https://github.com/UnboundSB/Vrutti/wiki/Shortcuts',
        'Video Tutorials': 'https://youtube.com/c/VruttiIDE',
        'Tips and Tricks': 'https://github.com/UnboundSB/Vrutti/wiki/Tips',
        'Join Us on YouTube': 'https://youtube.com/',
        'Search Feature Requests': 'https://github.com/UnboundSB/Vrutti/issues?q=label%3Aenhancement',
        'Report Issue': 'https://github.com/UnboundSB/Vrutti/issues',
        'View License': 'https://github.com/UnboundSB/Vrutti/blob/main/LICENSE',
        'Privacy Statement': 'https://github.com/UnboundSB/Vrutti/blob/main/PRIVACY.md'
      };

      if (editorActions.includes(detail.action)) {
        window.dispatchEvent(new CustomEvent('editor-action', { detail: { action: detail.action } }));
      } else if (externalLinks[detail.action]) {
        if ((window as any).vruttiOpenExternalUrl) {
           (window as any).vruttiOpenExternalUrl(externalLinks[detail.action]);
        }
      } else {
        // Fallback for not yet implemented items
        console.warn(`Action not yet implemented: ${detail.action}`);
        window.alert(`Feature Not Yet Implemented: ${detail.action}`);
      }
    }
  };

  private handleContextMenu = (e: CustomEvent) => {
    this.contextMenu = {
      x: e.detail.x,
      y: e.detail.y,
      path: e.detail.path,
      name: e.detail.name,
      isDirectory: e.detail.isDirectory
    };
  };

  private closeContextMenu = () => {
    this.contextMenu = null;
  };

  private handleExtensionSelected = (e: Event) => {
    this.activeExtension = (e as CustomEvent).detail;
  };

  private handleCloseExtensionDetails = () => {
    this.activeExtension = null;
  };

  private handleOpenFile = (e: CustomEvent) => {
    this.activeExtension = null;
    const layout = this.shadowRoot?.querySelector('#main-layout') as any;
    if (layout && layout.openFile) {
        layout.openFile(e.detail.path);
    }
  };

  private handleOpenGitGraph = async () => {
    if (!customElements.get('vrutti-git-graph')) {
        await import('./components/scm/vrutti-git-graph');
    }
    this.showGitGraph = true;
  };

  private handleCloseGitGraph = () => {
    this.showGitGraph = false;
  };

  private handleCloseSettings = () => {
    this.showSettings = false;
  };

  private toggleTerminal = () => {
    this.showTerminal = !this.showTerminal;
  };

  private handleGlobalKeydown = (e: KeyboardEvent) => {
    if (e.key.toLowerCase() === 'p' && (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      this.showQuickOpen = true;
    }
  };

  private handleSettingChanged = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && detail.key === 'appearance.glassmorphism') {
      if (detail.value) {
        document.body.classList.add('glass-mode');
        this.style.setProperty('--vrutti-backdrop-filter', 'blur(20px)');
      } else {
        document.body.classList.remove('glass-mode');
        this.style.setProperty('--vrutti-backdrop-filter', 'none');
      }
    }
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
      background-color: var(--vrutti-bg);
      color: var(--vrutti-text);
      font-family: 'Inter', -apple-system, sans-serif;
    }
    
    :host {
      display: block;
      height: 100vh;
      width: 100vw;
      background: transparent !important;
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
      position: relative;
      z-index: 9999;
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
      min-width: 0;
      overflow: hidden;
    }

    .context-menu {
      position: fixed;
      background: var(--vrutti-surface);
      border: 1px solid var(--vrutti-surface-border);
      border-radius: 6px;
      padding: 4px 0;
      min-width: 150px;
      z-index: 10000;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.5);
    }
    .context-menu-item {
      padding: 6px 12px;
      cursor: pointer;
      font-size: 13px;
    }
    .context-menu-item:hover {
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
    }
    
    vrutti-editor {
      flex: 1;
      background-color: transparent;
      min-height: 0;
      z-index: 1;
    }

    .terminal-panel {
      border-top: 1px solid var(--vrutti-surface-border);
      background: var(--vrutti-surface);
      display: flex;
      flex-direction: column;
      position: relative;
      min-width: 0;
      overflow: hidden;
    }
    .terminal-resizer {
      height: 4px;
      cursor: ns-resize;
      background: transparent;
      position: absolute;
      top: -2px;
      left: 0;
      right: 0;
      z-index: 100;
    }
    .terminal-resizer:hover {
      background: var(--vrutti-surface-border);
    }
    .terminal-body {
      display: flex;
      height: 100%;
      overflow: hidden;
      background: var(--vrutti-bg);
    }
    .terminal-instances {
      flex: 1;
      position: relative;
    }
    .terminal-tabs-container {
      width: 150px;
      border-left: 1px solid var(--vrutti-surface-border);
      display: flex;
      flex-direction: column;
      background: var(--vrutti-surface);
    }
    .terminal-tabs-actions {
      display: flex;
      justify-content: flex-end;
      padding: 4px;
      border-bottom: 1px solid var(--vrutti-surface-border);
    }
    .terminal-tabs-actions button {
      background: transparent;
      border: none;
      color: var(--vrutti-text);
      cursor: pointer;
      padding: 4px;
      border-radius: 4px;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tabs-actions button:hover {
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
    }
    .terminal-tabs-list {
      flex: 1;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
    }
    .terminal-tab {
      display: flex;
      align-items: center;
      padding: 4px 10px;
      cursor: pointer;
      color: var(--vrutti-text);
      font-size: 11px;
      user-select: none;
    }
    .terminal-tab:hover {
      background: var(--vrutti-surface-border);
    }
    .terminal-tab.active {
      color: var(--vrutti-text-bright);
      background: var(--vrutti-surface-border);
      border-left: 2px solid var(--vrutti-accent);
    }
    .terminal-tab-icon {
      margin-right: 6px;
      display: flex;
      align-items: center;
    }
    .terminal-tab-label {
      flex: 1;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .terminal-tab-close {
      display: none;
      padding: 2px;
      border-radius: 4px;
      color: var(--vrutti-text);
    }
    .terminal-tab:hover .terminal-tab-close {
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .terminal-tab-close:hover {
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
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
  `];

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
            <button title="Toggle Panel" @click=${this.toggleTerminal}>
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
            <vrutti-settings style="position: absolute; top: 0; left: 0; right: 0; bottom: 0; z-index: 500;"></vrutti-settings>
          ` : html`
            <div style="flex: 1; display: flex; flex-direction: column; position: relative;">
              ${this.activeExtension ? html`<vrutti-extension-details .extension=${this.activeExtension} style="position: absolute; top: 0; left: 0; right: 0; bottom: 0; z-index: 40;"></vrutti-extension-details>` : ''}
              <vrutti-editor-layout id="main-layout"></vrutti-editor-layout>
            </div>
            ${this.showTerminal ? html`
              <div class="terminal-panel" style="height: ${this.terminalHeight}px">
                <div class="terminal-resizer" @mousedown=${this.startTerminalResize}></div>
                <vrutti-panel @close-panel=${() => this.showTerminal = false}></vrutti-panel>
              </div>
            ` : ''}
          `}
          ${this.showGitGraph ? html`
              <vrutti-git-graph @close-git-graph=${() => this.showGitGraph = false}></vrutti-git-graph>
          ` : ''}

          ${this.showTaskPicker ? html`
              <vrutti-task-picker .mode=${this.taskPickerMode} @close-task-picker=${() => this.showTaskPicker = false}></vrutti-task-picker>
          ` : ''}

          ${this.showQuickOpen ? html`
            <vrutti-quick-open @close-quick-open=${() => this.showQuickOpen = false}></vrutti-quick-open>
          ` : ''}
        </div>
      </div>
      ${this.contextMenu ? html`
        <div class="context-menu" style="left: ${this.contextMenu.x}px; top: ${this.contextMenu.y}px;">
          ${!this.contextMenu.isDirectory ? html`
            <div class="context-menu-item" @click=${() => {
              const layout = this.shadowRoot?.querySelector('#main-layout') as any;
              if (layout && layout.openFile) {
                  layout.openFile(this.contextMenu!.path);
              }
              this.closeContextMenu();
            }}>Open File</div>
          ` : ''}
          <div class="context-menu-item" @click=${this.closeContextMenu}>Rename</div>
          <div class="context-menu-item" @click=${this.closeContextMenu}>Delete</div>
        </div>
      ` : ''}
      <vrutti-statusbar></vrutti-statusbar>
    `;
  }
}


