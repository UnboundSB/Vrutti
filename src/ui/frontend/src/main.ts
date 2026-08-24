import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './components/vrutti-sidebar';
import './components/vrutti-statusbar';
import './components/vrutti-menubar';
import './components/vrutti-panel';
import './components/vrutti-editor-layout';
import './components/vrutti-quick-pick';
import './components/vrutti-extension-details';
import { VruttiTaskManager } from './components/vrutti-task-manager';
import { themeBridge } from './ThemeBridge';
import { registry } from './core/Registry';
import { registerCoreContributions } from './core/core-contributions';

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
  @state() private showQuickPick = false;
  @state() private quickPickPrefix = '';
  @state() private showTerminal = false;
  @state() private terminalHeight = 300;
  
  @state() private showOpenFolderModal = false;
  @state() private pendingFolderPath = '';

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
    registerCoreContributions();
    
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
        
        if (msg.method === 'extensions/injections') {
            const injections = msg.params || [];
            for (const inj of injections) {
                if (inj.type === 'css') {
                    const style = document.createElement('style');
                    style.innerHTML = inj.content || '';
                    document.head.appendChild(style);
                } else if (inj.type === 'js') {
                    const script = document.createElement('script');
                    script.innerHTML = inj.content || '';
                    document.head.appendChild(script);
                }
            }
        } else if (msg.method === 'extensions/installed') {
            const exts = msg.params || [];
            for (const ext of exts) {
                if (ext.contributes && ext.contributes.viewsContainers && ext.contributes.viewsContainers.activitybar) {
                    for (const container of ext.contributes.viewsContainers.activitybar) {
                        registry.registerActivityBar({
                            id: container.id,
                            title: container.title,
                            iconContent: container.iconContent || '',
                            order: 100
                        });
                        if (container.iconPath && (window as any).vruttiReadFile) {
                            (window as any).vruttiReadFile(container.iconPath).then((content: string) => {
                                if (content) {
                                    registry.registerActivityBar({
                                        id: container.id,
                                        title: container.title,
                                        iconContent: content,
                                        order: 100
                                    });
                                }
                            }).catch(console.error);
                        }
                        const extViews = (ext.contributes.views && ext.contributes.views[container.id]) || [];
                        for (const view of extViews) {
                            registry.registerView({
                                id: view.id,
                                containerId: container.id,
                                name: view.name,
                                component: 'vrutti-webview'
                            });
                        }
                    }
                }
            }
        } else if (msg.method === 'run/output') {
           if ((window as any).vruttiWriteOutput) {
               (window as any).vruttiWriteOutput('Execution', msg.params.text);
           }
        } else if (msg.method === 'menu/action') {
           if (msg.params && msg.params.command) {
               registry.executeCommand(msg.params.command);
           } else if (msg.params && msg.params.action) {
               registry.executeCommand(msg.params.action);
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

    this.registerCommands();
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
      (window as any).vruttiGetSettings().then((payload: any) => {
        try {
          const settings = typeof payload === 'string' ? JSON.parse(payload) : payload;
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

  private registerCommands() {
    registry.registerCommand('vrutti.action.preferences', async () => {
      if (!customElements.get('vrutti-settings')) {
        await import('./components/vrutti-settings');
      }
      this.showSettings = true;
    });

    registry.registerCommand('vrutti.action.appearance', () => { this.showSettings = true; });
    registry.registerCommand('vrutti.action.editorLayout', () => { this.showSettings = true; });

    registry.registerCommand('vrutti.action.toggleWordWrap', () => {
      this.toggleSetting('editor.wordWrap');
    });

    registry.registerCommand('vrutti.action.autoSave', () => {
      this.toggleSetting('files.autoSave');
    });

    registry.registerCommand('vrutti.action.openFolder', async () => {
      if ((window as any).vruttiOpenFolderDialog) {
        try {
          const json = await (window as any).vruttiOpenFolderDialog();
          if (json && json.path && json.path !== "") {
            if ((window as any).currentWorkspace) {
              this.pendingFolderPath = json.path;
              this.showOpenFolderModal = true;
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
    });

    registry.registerCommand('vrutti.action.toggleTerminal', () => {
      this.toggleTerminal();
    });

    const showSidebarTab = (tab: string) => {
      const sidebar = this.shadowRoot?.querySelector('vrutti-sidebar') as any;
      if (sidebar && sidebar.selectTab) sidebar.selectTab(tab);
    };

    registry.registerCommand('vrutti.action.showExplorer', () => showSidebarTab('explorer'));
    registry.registerCommand('vrutti.action.showSearch', () => showSidebarTab('search'));
    registry.registerCommand('vrutti.action.showScm', () => showSidebarTab('scm'));
    registry.registerCommand('vrutti.action.showDebug', () => showSidebarTab('debug'));
    registry.registerCommand('vrutti.action.showExtensions', () => showSidebarTab('extensions'));

    registry.registerCommand('vrutti.action.toggleDevTools', () => {
      if ((window as any).vruttiToggleDevTools) {
        (window as any).vruttiToggleDevTools();
      }
    });

    registry.registerCommand('vrutti.action.newFile', () => {
      const layout = this.shadowRoot?.querySelector('#main-layout') as any;
      if (layout && layout.openFile) {
          layout.openFile('Untitled-1');
      }
    });

    registry.registerCommand('vrutti.action.openFile', async () => {
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
    });

    registry.registerCommand('vrutti.action.closeEditor', () => {
      const layout = this.shadowRoot?.querySelector('#main-layout') as any;
      if (layout && layout.closeActiveEditor) {
          layout.closeActiveEditor();
      }
    });

    registry.registerCommand('vrutti.action.showCommands', () => { this.quickPickPrefix = '>'; this.showQuickPick = true; });
    registry.registerCommand('vrutti.action.openView', () => { this.quickPickPrefix = '>'; this.showQuickPick = true; });
    registry.registerCommand('vrutti.action.quickOpen', () => { this.quickPickPrefix = ''; this.showQuickPick = true; });

    registry.registerCommand('vrutti.action.help.about', () => {
      window.alert('Vrutti IDE\\nVersion 1.0.0\\nBuilt by UnboundSB');
    });

    registry.registerCommand('vrutti.action.help.checkUpdates', () => {
      window.alert('There are currently no updates available.');
    });

    registry.registerCommand('vrutti.action.terminal.new', () => { this.showTerminal = true; });

    registry.registerCommand('vrutti.action.tasks.runTask', () => {
      this.quickPickPrefix = 'task ';
      this.showQuickPick = true;
    });
    registry.registerCommand('vrutti.action.tasks.showActive', () => {
      this.quickPickPrefix = 'task active ';
      this.showQuickPick = true;
    });
    registry.registerCommand('vrutti.action.tasks.configureDefault', () => {
      this.quickPickPrefix = 'task default ';
      this.showQuickPick = true;
    });
    registry.registerCommand('vrutti.action.tasks.configure', () => {
      VruttiTaskManager.ensureTasksFileExists().then(path => {
          if (path) {
              window.dispatchEvent(new CustomEvent('open-file', { detail: { path: path, name: 'tasks.json' } }));
          }
      });
    });
    registry.registerCommand('vrutti.action.tasks.build', () => {
      VruttiTaskManager.runDefaultBuildTask().then(ran => {
          if (!ran) {
              this.quickPickPrefix = 'task default ';
              this.showQuickPick = true;
          }
      });
    });

    registry.registerCommand('vrutti.action.closeWindow', () => {
        if ((window as any).closeWindow) (window as any).closeWindow();
    });
    registry.registerCommand('vrutti.action.newWindow', () => {
        if ((window as any).vruttiOpenNewWindow) (window as any).vruttiOpenNewWindow('');
    });
    registry.registerCommand('vrutti.action.saveAs', () => {
        window.alert('Save As not implemented');
    });
    registry.registerCommand('vrutti.action.saveAll', () => {
        window.alert('Save All not implemented');
    });

    const externalLinks: Record<string, string> = {
      'vrutti.action.help.welcome': 'https://github.com/UnboundSB/Vrutti',
      'vrutti.action.help.documentation': new URL('./help.html', window.location.href).href,
      'vrutti.action.help.playground': 'https://github.com/UnboundSB/Vrutti/wiki/Playground',
      'vrutti.action.help.releaseNotes': 'https://github.com/UnboundSB/Vrutti/releases',
      'vrutti.action.help.keyboardShortcuts': 'https://github.com/UnboundSB/Vrutti/wiki/Shortcuts',
      'vrutti.action.help.videoTutorials': 'https://youtube.com/c/VruttiIDE',
      'vrutti.action.help.tipsAndTricks': 'https://github.com/UnboundSB/Vrutti/wiki/Tips',
      'vrutti.action.help.youtube': 'https://youtube.com/',
      'vrutti.action.help.featureRequests': 'https://github.com/UnboundSB/Vrutti/issues?q=label%3Aenhancement',
      'vrutti.action.help.reportIssue': 'https://github.com/UnboundSB/Vrutti/issues',
      'vrutti.action.help.license': 'https://github.com/UnboundSB/Vrutti/blob/main/LICENSE',
      'vrutti.action.help.privacy': 'https://github.com/UnboundSB/Vrutti/blob/main/PRIVACY.md'
    };
    
    for (const [cmd, url] of Object.entries(externalLinks)) {
        registry.registerCommand(cmd, () => {
            if ((window as any).vruttiOpenExternalUrl) {
                (window as any).vruttiOpenExternalUrl(url);
            }
        });
    }

    const editorActionMapping: Record<string, string> = {
      'vrutti.action.editor.undo': 'Undo',
      'vrutti.action.editor.redo': 'Redo',
      'vrutti.action.editor.cut': 'Cut',
      'vrutti.action.editor.copy': 'Copy',
      'vrutti.action.editor.paste': 'Paste',
      'vrutti.action.editor.find': 'Find',
      'vrutti.action.editor.replace': 'Replace',
      'vrutti.action.findInFiles': 'Find in Files',
      'vrutti.action.replaceInFiles': 'Replace in Files',
      'vrutti.action.editor.selectAll': 'Select All',
      'vrutti.action.editor.expandSelection': 'Expand Selection',
      'vrutti.action.editor.shrinkSelection': 'Shrink Selection',
      'vrutti.action.editor.copyLineUp': 'Copy Line Up',
      'vrutti.action.editor.copyLineDown': 'Copy Line Down',
      'vrutti.action.editor.moveLineUp': 'Move Line Up',
      'vrutti.action.editor.moveLineDown': 'Move Line Down',
      'vrutti.action.editor.duplicateSelection': 'Duplicate Selection',
      'vrutti.action.editor.addCursorAbove': 'Add Cursor Above',
      'vrutti.action.editor.addCursorBelow': 'Add Cursor Below',
      'vrutti.action.editor.addCursorsToLineEnds': 'Add Cursors to Line Ends',
      'vrutti.action.editor.addNextOccurrence': 'Add Next Occurrence',
      'vrutti.action.editor.addPreviousOccurrence': 'Add Previous Occurrence',
      'vrutti.action.editor.selectAllOccurrences': 'Select All Occurrences',
      'vrutti.action.editor.toggleLineComment': 'Toggle Line Comment',
      'vrutti.action.editor.toggleBlockComment': 'Toggle Block Comment',
      'vrutti.action.editor.gotoDefinition': 'Go to Definition',
      'vrutti.action.editor.gotoDeclaration': 'Go to Declaration',
      'vrutti.action.editor.gotoTypeDefinition': 'Go to Type Definition',
      'vrutti.action.editor.gotoImplementations': 'Go to Implementations',
      'vrutti.action.editor.gotoReferences': 'Go to References',
      'vrutti.action.editor.nextProblem': 'Next Problem',
      'vrutti.action.editor.previousProblem': 'Previous Problem',
      'vrutti.action.editor.nextChange': 'Next Change',
      'vrutti.action.editor.previousChange': 'Previous Change',
      'vrutti.action.editor.toggleBreakpoint': 'Toggle Breakpoint',
      'vrutti.action.save': 'Save',
      'vrutti.action.goBack': 'Back',
      'vrutti.action.goForward': 'Forward',
      'vrutti.action.showAllSymbols': 'Go to Symbol in Workspace',
      'vrutti.action.gotoLine': 'Go to Line/Column',
      'vrutti.action.debug.start': 'Start Debugging',
      'vrutti.action.debug.run': 'Run Without Debugging',
      'vrutti.action.debug.stop': 'Stop Debugging',
      'vrutti.action.debug.restart': 'Restart Debugging',
      'vrutti.action.debug.stepOver': 'Step Over',
      'vrutti.action.debug.stepInto': 'Step Into',
      'vrutti.action.debug.stepOut': 'Step Out',
      'vrutti.action.debug.continue': 'Continue',
      'vrutti.action.debug.newBreakpoint': 'New Breakpoint',
      'vrutti.action.debug.openConfigurations': 'Open Configurations',
      'vrutti.action.debug.addConfiguration': 'Add Configuration',
      'vrutti.action.editor.switchMultiCursorModifier': 'Switch to Ctrl+Click for Multi-Cursor',
      'vrutti.action.editor.columnSelectionMode': 'Column Selection Mode',
    };

    for (const [cmd, actionLabel] of Object.entries(editorActionMapping)) {
        registry.registerCommand(cmd, () => {
            window.dispatchEvent(new CustomEvent('editor-action', { detail: { action: actionLabel } }));
        });
    }
  }
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
    if (e.defaultPrevented) return;
    if (['Shift', 'Control', 'Alt', 'Meta'].includes(e.key)) return;
    
    const isCtrl = e.ctrlKey || e.metaKey;
    const isShift = e.shiftKey;
    const isAlt = e.altKey;
    
    let key = e.key.toUpperCase();
    
    const parts = [];
    if (isCtrl) parts.push('Ctrl');
    if (isShift) parts.push('Shift');
    if (isAlt) parts.push('Alt');
    parts.push(key);
    
    const keyStr = parts.join('+');
    
    const bindings = registry.getKeybindings();
    const match = bindings.find(b => b.key.toUpperCase() === keyStr);
    
    if (match) {
      e.preventDefault();
      registry.executeCommand(match.command);
    }
  };

  private handleSettingChanged = async (e: Event) => {
    const detail = (e as CustomEvent).detail;
    if (detail && detail.key === 'appearance.transparencyEffects') {
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

  private async handleOpenFolderAction(action: 'this' | 'new' | 'cancel') {
    this.showOpenFolderModal = false;
    if (action === 'cancel') return;

    if (action === 'new') {
      if ((window as any).vruttiOpenNewWindow) {
        await (window as any).vruttiOpenNewWindow(this.pendingFolderPath);
      }
    } else if (action === 'this') {
      window.dispatchEvent(new CustomEvent('workspace-changed', {
        detail: { path: this.pendingFolderPath }
      }));
    }
    this.pendingFolderPath = '';
  }

  static styles = [globalHoverStyle, css`
    .modal-overlay {
      position: fixed;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0, 0, 0, 0.5);
      z-index: 1000;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .modal-content {
      background: var(--vrutti-surface, #1e1e1e);
      border: 1px solid var(--vrutti-surface-border, #2d2d2d);
      border-radius: 6px;
      padding: 20px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.5);
      width: 400px;
      color: var(--vrutti-text-bright, #ffffff);
    }
    .modal-title { font-size: 14px; font-weight: 600; margin-bottom: 10px; }
    .modal-message { font-size: 13px; color: var(--vrutti-text, #cccccc); margin-bottom: 20px; word-break: break-all; }
    .modal-actions { display: flex; justify-content: flex-end; gap: 10px; }
    .modal-btn {
      background: var(--vrutti-bg, #1a1a1a);
      color: var(--vrutti-text-bright, #fff);
      border: 1px solid var(--vrutti-surface-border, #333);
      padding: 6px 12px; border-radius: 3px; cursor: pointer; font-size: 13px;
    }
    .modal-btn:hover { background: var(--vrutti-surface, #2d2d2d); }
    .modal-btn.primary { background: var(--vrutti-accent, #007acc); border-color: var(--vrutti-accent, #007acc); }
    .modal-btn.primary:hover { background: #005a9e; }

    :host {
      display: flex;
      flex-direction: column;
      width: 100vw;
      height: 100vh;
      overflow: hidden;
      background: transparent !important;
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
      backdrop-filter: var(--vrutti-backdrop-filter, none);
      -webkit-backdrop-filter: var(--vrutti-backdrop-filter, none);
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
      z-index: 10000;
    }
    .terminal-resizer {
      height: 4px;
      cursor: ns-resize;
      background: transparent;
      position: absolute;
      top: -2px;
      left: 0;
      right: 0;
      z-index: 10000;
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
            <div style="flex: 1; display: flex; flex-direction: column; position: relative; min-height: 0; overflow: hidden;">
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

          ${this.showQuickPick ? html`
            <vrutti-quick-pick .initialPrefix=${this.quickPickPrefix} @close-quick-pick=${() => this.showQuickPick = false}></vrutti-quick-pick>
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
      ${this.showOpenFolderModal ? html`
        <div class="modal-overlay">
          <div class="modal-content">
            <div class="modal-title">Open Folder</div>
            <div class="modal-message">Do you want to open <strong>${this.pendingFolderPath}</strong> in this window, or in a new window?</div>
            <div class="modal-actions">
              <button class="modal-btn" @click=${() => this.handleOpenFolderAction('cancel')}>Cancel</button>
              <button class="modal-btn" @click=${() => this.handleOpenFolderAction('this')}>This Window</button>
              <button class="modal-btn primary" @click=${() => this.handleOpenFolderAction('new')}>New Window</button>
            </div>
          </div>
        </div>
      ` : ''}
      <vrutti-statusbar></vrutti-statusbar>
    `;
  }
}


