import { registry } from './Registry';
import { VruttiTaskManager } from '../components/vrutti-task-manager';
import { 
    icon_files, 
    icon_search, 
    icon_source_control, 
    icon_debug_alt, 
    icon_extensions,
    icon_git_branch,
    icon_sync,
    icon_cloud_upload,
    icon_cloud_download,
    icon_add,
    icon_remove,
    icon_replace_all
} from '../components/codicons';

export function registerCoreContributions() {
    // ActivityBar Contributions
    registry.registerActivityBar({
        id: 'explorer',
        title: 'Explorer',
        iconContent: icon_files,
        order: 10
    });
    registry.registerActivityBar({
        id: 'search',
        title: 'Search',
        iconContent: icon_search,
        order: 20
    });
    registry.registerActivityBar({
        id: 'scm',
        title: 'Source Control',
        iconContent: icon_source_control,
        order: 30
    });
    registry.registerActivityBar({
        id: 'debug',
        title: 'Run and Debug',
        iconContent: icon_debug_alt,
        order: 40
    });
    registry.registerActivityBar({
        id: 'extensions',
        title: 'Extensions',
        iconContent: icon_extensions,
        order: 50
    });

    // Views Contributions
    registry.registerView({
        id: 'explorer-view',
        containerId: 'explorer',
        name: 'Explorer',
        component: 'vrutti-explorer-view'
    });
    registry.registerView({
        id: 'search-view',
        containerId: 'search',
        name: 'Search',
        component: 'vrutti-search'
    });
    registry.registerView({
        id: 'scm-view',
        containerId: 'scm',
        name: 'Source Control',
        component: 'vrutti-scm'
    });
    registry.registerView({
        id: 'debug-view',
        containerId: 'debug',
        name: 'Run and Debug',
        component: 'vrutti-debug-sidebar'
    });
    registry.registerView({
        id: 'extensions-view',
        containerId: 'extensions',
        name: 'Extensions',
        component: 'vrutti-extensions'
    });
    // Panel Contributions
    registry.registerPanel({ id: 'PROBLEMS', title: 'PROBLEMS', order: 10 });
    registry.registerPanel({ id: 'OUTPUT', title: 'OUTPUT', order: 20, component: 'vrutti-output-view' });
    registry.registerPanel({ id: 'DEBUG CONSOLE', title: 'DEBUG CONSOLE', order: 30, component: 'vrutti-debug-console' });
    registry.registerPanel({ id: 'TERMINAL', title: 'TERMINAL', order: 40, component: 'vrutti-terminal-view' });
    registry.registerPanel({ id: 'PORTS', title: 'PORTS', order: 50 });

    // Output Channels
    registry.registerOutputChannel('System');
    registry.registerOutputChannel('Extension Host');
    registry.registerOutputChannel('Tasks');

    // Status Bar Contributions
    registry.registerStatusBar({
        id: 'git-branch',
        alignment: 'left',
        order: 10,
        text: 'main',
        iconContent: icon_git_branch,
        tooltip: 'Git Branch'
    });
    registry.registerStatusBar({
        id: 'errors-warnings',
        alignment: 'left',
        order: 20,
        component: 'errors-warnings'
    });
    
    registry.registerStatusBar({
        id: 'cursor-position',
        alignment: 'right',
        order: 10,
        text: 'Ln 1, Col 1',
        tooltip: 'Go to Line/Column'
    });
    registry.registerStatusBar({
        id: 'indentation',
        alignment: 'right',
        order: 20,
        tooltip: 'Select Indentation'
    });
    registry.registerStatusBar({
        id: 'encoding',
        alignment: 'right',
        order: 30,
        tooltip: 'Select Encoding'
    });
    registry.registerStatusBar({
        id: 'eol',
        alignment: 'right',
        order: 40,
        tooltip: 'Select End of Line Sequence'
    });
    registry.registerStatusBar({
        id: 'language',
        alignment: 'right',
        order: 50,
        text: 'TypeScript',
        tooltip: 'Select Language Mode'
    });

    // Menubar Contributions (File)
    registry.registerMenu({
        id: 'file',
        title: 'File',
        order: 10,
        items: [
            { label: 'New File', command: 'vrutti.action.newFile', order: 10 },
            { label: 'New Window', command: 'vrutti.action.newWindow', order: 20 },
            { label: 'separator1', separator: true, order: 30 },
            { label: 'Open File', command: 'vrutti.action.openFile', order: 40 },
            { label: 'Open Folder...', command: 'vrutti.action.openFolder', order: 50 },
            { label: 'Open Recent', command: 'vrutti.action.openRecent', order: 60 },
            { label: 'separator2', separator: true, order: 70 },
            { label: 'Save', command: 'vrutti.action.save', order: 80 },
            { label: 'Save As', command: 'vrutti.action.saveAs', order: 90 },
            { label: 'Save All', command: 'vrutti.action.saveAll', order: 100 },
            { label: 'separator3', separator: true, order: 110 },
            { label: 'Auto Save', command: 'vrutti.action.autoSave', order: 120 },
            { label: 'Preferences', command: 'vrutti.action.preferences', order: 130 },
            { label: 'separator4', separator: true, order: 140 },
            { label: 'Close Editor', command: 'vrutti.action.closeEditor', order: 150 },
            { label: 'Close Folder', command: 'vrutti.action.closeFolder', order: 160 },
            { label: 'Close Window', command: 'vrutti.action.closeWindow', order: 170 },
            { label: 'separator5', separator: true, order: 180 },
            { label: 'Exit', command: 'vrutti.action.exit', order: 190 }
        ]
    });

    // Menubar Contributions (Edit)
    registry.registerMenu({
        id: 'edit',
        title: 'Edit',
        order: 20,
        items: [
            { label: 'Undo', command: 'vrutti.action.editor.undo', order: 10 },
            { label: 'Redo', command: 'vrutti.action.editor.redo', order: 20 },
            { label: 'sep1', separator: true, order: 30 },
            { label: 'Cut', command: 'vrutti.action.editor.cut', order: 40 },
            { label: 'Copy', command: 'vrutti.action.editor.copy', order: 50 },
            { label: 'Paste', command: 'vrutti.action.editor.paste', order: 60 },
            { label: 'sep2', separator: true, order: 70 },
            { label: 'Find', command: 'vrutti.action.editor.find', order: 80 },
            { label: 'Replace', command: 'vrutti.action.editor.replace', order: 90 },
            { label: 'sep3', separator: true, order: 100 },
            { label: 'Find in Files', command: 'vrutti.action.findInFiles', order: 110 },
            { label: 'Replace in Files', command: 'vrutti.action.replaceInFiles', order: 120 },
            { label: 'sep4', separator: true, order: 130 },
            { label: 'Toggle Line Comment', command: 'vrutti.action.editor.toggleLineComment', order: 140 },
            { label: 'Toggle Block Comment', command: 'vrutti.action.editor.toggleBlockComment', order: 150 }
        ]
    });

    // Selection Menu
    registry.registerMenu({
        id: 'selection',
        title: 'Selection',
        order: 30,
        items: [
            { label: 'Select All', command: 'vrutti.action.editor.selectAll' },
            { label: 'Expand Selection', command: 'vrutti.action.editor.expandSelection' },
            { label: 'Shrink Selection', command: 'vrutti.action.editor.shrinkSelection' },
            { label: 'sep1', separator: true },
            { label: 'Copy Line Up', command: 'vrutti.action.editor.copyLineUp' },
            { label: 'Copy Line Down', command: 'vrutti.action.editor.copyLineDown' },
            { label: 'Move Line Up', command: 'vrutti.action.editor.moveLineUp' },
            { label: 'Move Line Down', command: 'vrutti.action.editor.moveLineDown' },
            { label: 'Duplicate Selection', command: 'vrutti.action.editor.duplicateSelection' },
            { label: 'sep2', separator: true },
            { label: 'Add Cursor Above', command: 'vrutti.action.editor.addCursorAbove' },
            { label: 'Add Cursor Below', command: 'vrutti.action.editor.addCursorBelow' },
            { label: 'Add Cursors to Line Ends', command: 'vrutti.action.editor.addCursorsToLineEnds' },
            { label: 'Add Next Occurrence', command: 'vrutti.action.editor.addNextOccurrence' },
            { label: 'Add Previous Occurrence', command: 'vrutti.action.editor.addPreviousOccurrence' },
            { label: 'Select All Occurrences', command: 'vrutti.action.editor.selectAllOccurrences' },
            { label: 'sep3', separator: true },
            { label: 'Switch to Ctrl+Click for Multi-Cursor', command: 'vrutti.action.editor.switchMultiCursorModifier' },
            { label: 'Column Selection Mode', command: 'vrutti.action.editor.columnSelectionMode' }
        ]
    });

    // View Menu
    registry.registerMenu({
        id: 'view',
        title: 'View',
        order: 40,
        items: [
            { label: 'Command Palette', command: 'vrutti.action.showCommands' },
            { label: 'Open View', command: 'vrutti.action.openView' },
            { label: 'sep1', separator: true },
            { label: 'Appearance', command: 'vrutti.action.appearance' },
            { label: 'Editor Layout', command: 'vrutti.action.editorLayout' },
            { label: 'sep2', separator: true },
            { label: 'Explorer', command: 'vrutti.action.showExplorer' },
            { label: 'Search', command: 'vrutti.action.showSearch' },
            { label: 'Source Control', command: 'vrutti.action.showScm' },
            { label: 'Run', command: 'vrutti.action.showDebug' },
            { label: 'Extensions', command: 'vrutti.action.showExtensions' },
            { label: 'sep3', separator: true },
            { label: 'Problems', command: 'vrutti.action.showProblems' },
            { label: 'Output', command: 'vrutti.action.showOutput' },
            { label: 'Debug Console', command: 'vrutti.action.showDebugConsole' },
            { label: 'Terminal', command: 'vrutti.action.toggleTerminal' },
            { label: 'sep4', separator: true },
            { label: 'Word Wrap', command: 'vrutti.action.toggleWordWrap' }
        ]
    });

    // Go Menu
    registry.registerMenu({
        id: 'go',
        title: 'Go',
        order: 50,
        items: [
            { label: 'Back', command: 'vrutti.action.goBack' },
            { label: 'Forward', command: 'vrutti.action.goForward' },
            { label: 'sep1', separator: true },
            { label: 'Go to File', command: 'vrutti.action.quickOpen' },
            { label: 'Go to Symbol in Workspace', command: 'vrutti.action.showAllSymbols' },
            { label: 'sep2', separator: true },
            { label: 'Go to Line/Column', command: 'vrutti.action.gotoLine' },
            { label: 'Go to Definition', command: 'vrutti.action.editor.gotoDefinition' },
            { label: 'Go to Declaration', command: 'vrutti.action.editor.gotoDeclaration' },
            { label: 'Go to Type Definition', command: 'vrutti.action.editor.gotoTypeDefinition' },
            { label: 'Go to Implementations', command: 'vrutti.action.editor.gotoImplementations' },
            { label: 'Go to References', command: 'vrutti.action.editor.gotoReferences' },
            { label: 'sep3', separator: true },
            { label: 'Next Problem', command: 'vrutti.action.editor.nextProblem' },
            { label: 'Previous Problem', command: 'vrutti.action.editor.previousProblem' },
            { label: 'Next Change', command: 'vrutti.action.editor.nextChange' },
            { label: 'Previous Change', command: 'vrutti.action.editor.previousChange' }
        ]
    });

    // Run Menu
    registry.registerMenu({
        id: 'run',
        title: 'Run',
        order: 60,
        items: [
            { label: 'Start Debugging', command: 'vrutti.action.debug.start' },
            { label: 'Run Without Debugging', command: 'vrutti.action.debug.run' },
            { label: 'Stop Debugging', command: 'vrutti.action.debug.stop' },
            { label: 'Restart Debugging', command: 'vrutti.action.debug.restart' },
            { label: 'sep1', separator: true },
            { label: 'Open Configurations', command: 'vrutti.action.debug.openConfigurations' },
            { label: 'Add Configuration', command: 'vrutti.action.debug.addConfiguration' },
            { label: 'sep2', separator: true },
            { label: 'Step Over', command: 'vrutti.action.debug.stepOver' },
            { label: 'Step Into', command: 'vrutti.action.debug.stepInto' },
            { label: 'Step Out', command: 'vrutti.action.debug.stepOut' },
            { label: 'Continue', command: 'vrutti.action.debug.continue' },
            { label: 'sep3', separator: true },
            { label: 'Toggle Breakpoint', command: 'vrutti.action.editor.toggleBreakpoint' },
            { label: 'New Breakpoint', command: 'vrutti.action.debug.newBreakpoint' }
        ]
    });

    // Terminal Menu
    registry.registerMenu({
        id: 'terminal',
        title: 'Terminal',
        order: 70,
        items: [
            { label: 'New Terminal', command: 'vrutti.action.terminal.new' },
            { label: 'sep1', separator: true },
            { label: 'Run Task', command: 'vrutti.action.tasks.runTask' },
            { label: 'Build Task', command: 'vrutti.action.tasks.build' },
            { label: 'Active Tasks', command: 'vrutti.action.tasks.showActive' },
            { label: 'sep2', separator: true },
            { label: 'Configure Tasks', command: 'vrutti.action.tasks.configure' },
            { label: 'Configure Default Build Task', command: 'vrutti.action.tasks.configureDefault' }
        ]
    });

    // Help Menu
    registry.registerMenu({
        id: 'help',
        title: 'Help',
        order: 50,
        items: [
            { label: 'Welcome', command: 'vrutti.action.help.welcome', order: 10 },
            { label: 'Documentation', command: 'vrutti.action.help.documentation', order: 20 },
            { label: 'Playground', command: 'vrutti.action.help.playground', order: 30 },
            { label: 'Release Notes', command: 'vrutti.action.help.releaseNotes', order: 40 },
            { separator: true, order: 45 },
            { label: 'Keyboard Shortcuts Reference', command: 'vrutti.action.help.keyboardShortcuts', order: 50 },
            { label: 'Video Tutorials', command: 'vrutti.action.help.videoTutorials', order: 60 },
            { label: 'Tips and Tricks', command: 'vrutti.action.help.tipsAndTricks', order: 70 },
            { separator: true, order: 75 },
            { label: 'Join us on YouTube', command: 'vrutti.action.help.youtube', order: 80 },
            { separator: true, order: 85 },
            { label: 'Feature Request', command: 'vrutti.action.help.featureRequests', order: 90 },
            { label: 'Report Issue', command: 'vrutti.action.help.reportIssue', order: 100 },
            { separator: true, order: 105 },
            { label: 'View License', command: 'vrutti.action.help.license', order: 110 },
            { label: 'Privacy Statement', command: 'vrutti.action.help.privacy', order: 120 }
        ]
    });

    // Explorer Context Menu
    registry.registerMenu({
        id: 'explorer/context',
        items: [
            { label: 'Open File', command: 'explorer.openFile', when: '!isDirectory', order: 10 },
            { label: 'Rename', command: 'explorer.rename', order: 20 },
            { label: 'Delete', command: 'explorer.delete', order: 30 }
        ]
    });

    // SCM Title Menu
    registry.registerMenu({
        id: 'scm/title',
        items: [
            { label: 'View Git Graph', command: 'scm.viewGraph', order: 10, iconContent: '<svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M4 2a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 2Zm10 12a2 2 0 1 1-1.85-2.75l-1.01-.505a.75.75 0 0 1 .673-1.343l1.01.505A2 2 0 0 1 14 14ZM4 10a2 2 0 1 1-1.85 2.75l-1.01.505a.75.75 0 0 1-.673-1.343l1.01-.505A2 2 0 0 1 4 10Zm5-5a2 2 0 1 1-1.85 2.75l-3.02 1.51a.75.75 0 0 1-.673-1.343l3.02-1.51A2 2 0 0 1 9 5Z"/></svg>' },
            { label: 'Refresh', command: 'scm.refresh', order: 20, iconContent: icon_sync },
            { label: 'Pull', command: 'scm.pull', order: 30, iconContent: icon_cloud_download },
            { label: 'Push', command: 'scm.push', order: 40, iconContent: icon_cloud_upload }
        ]
    });

    // SCM Resource State Menus
    registry.registerMenu({
        id: 'scm/resourceState/staged',
        items: [
            { label: 'Unstage Changes', command: 'scm.unstageFile', order: 10, iconContent: icon_remove }
        ]
    });

    registry.registerMenu({
        id: 'scm/resourceState/unstaged',
        items: [
            { label: 'Stage Changes', command: 'scm.stageFile', order: 10, iconContent: icon_add }
        ]
    });

    // Default Keybindings
    registry.registerKeybinding({ key: 'Ctrl+Shift+P', command: 'vrutti.action.showCommands' });
    registry.registerKeybinding({ key: 'Ctrl+P', command: 'vrutti.action.quickOpen' });
    registry.registerKeybinding({ key: 'Ctrl+S', command: 'vrutti.action.save' });
    registry.registerKeybinding({ key: 'Ctrl+Shift+S', command: 'vrutti.action.saveAs' });
    registry.registerKeybinding({ key: 'Ctrl+O', command: 'vrutti.action.openFile' });
    registry.registerKeybinding({ key: 'Ctrl+Shift+O', command: 'vrutti.action.openFolder' });
    registry.registerKeybinding({ key: 'Ctrl+W', command: 'vrutti.action.closeEditor' });
    registry.registerKeybinding({ key: 'Ctrl+F', command: 'vrutti.action.editor.find' });
    registry.registerKeybinding({ key: 'Ctrl+H', command: 'vrutti.action.editor.replace' });
    registry.registerKeybinding({ key: 'Ctrl+`', command: 'vrutti.action.toggleTerminal' });
    registry.registerKeybinding({ key: 'F5', command: 'vrutti.action.debug.start' });

    // Settings Categories
    registry.registerSettingsCategory({ id: 'General', title: 'General', order: 10 });
    registry.registerSettingsCategory({ id: 'Appearance', title: 'Appearance', order: 20 });
    registry.registerSettingsCategory({ id: 'Editor', title: 'Editor', order: 30 });
    registry.registerSettingsCategory({ id: 'Theme', title: 'Theme', order: 40 });
    registry.registerSettingsCategory({ id: 'Keybindings', title: 'Keybindings', order: 50 });

    // Configuration
    registry.registerConfiguration({
        id: 'General',
        title: 'General',
        order: 10,
        properties: {
            'telemetry.enableTelemetry': {
                type: 'boolean',
                default: false,
                description: 'Enable telemetry to send crash reports and usage data.'
            }
        }
    });

    registry.registerConfiguration({
        id: 'Editor',
        title: 'Editor',
        order: 20,
        properties: {
            'editor.fontSize': { type: 'number', default: 14, description: 'Controls the font size in pixels.' },
            'editor.fontFamily': { type: 'string', default: "'Fira Code', monospace", description: 'Controls the font family.' },
            'editor.wordWrap': { type: 'boolean', default: false, description: 'Controls whether lines should wrap.' },
            'editor.tabSize': { type: 'number', default: 4, description: 'The number of spaces a tab is equal to.' },
            'editor.insertSpaces': { type: 'boolean', default: true, description: 'Insert spaces when pressing Tab.' },
            'editor.minimap.enabled': { type: 'boolean', default: true, description: 'Controls whether the minimap is shown.' },
            'files.autoSave': { type: 'boolean', default: false, description: 'Controls auto save of dirty files.' }
        }
    });

    registry.registerConfiguration({
        id: 'Appearance',
        title: 'Appearance',
        order: 30,
        properties: {
            'appearance.transparencyEffects': { type: 'boolean', default: false, description: 'Enable window transparency effects.' },
            'appearance.customBackgroundType': { type: 'enum', enum: ['none', 'image', 'video'], default: 'none', description: 'Type of custom background for empty editor (none, image, video).' },
            'appearance.customBackgroundPath': { type: 'string', default: '', description: 'File path or URL for the custom background image/video.' },
            'appearance.customBackgroundOpacity': { type: 'number', default: 0.15, description: 'Opacity of the custom background (0.0 to 1.0).' }
        }
    });

    registry.registerConfiguration({
        id: 'Theme',
        title: 'Theme',
        order: 40,
        properties: {
            'workbench.colorTheme': { type: 'string', default: 'Default Dark+', description: 'Specifies the color theme used in the workbench.' },
            'workbench.iconTheme': { type: 'string', default: 'Material Icon Theme', description: 'Specifies the file icon theme used in the workbench or "default" to use built-in.' }
        }
    });

    // Quick Pick Providers
    registry.registerQuickPickProvider({
        prefix: '',
        description: 'Search files by name...',
        getResults: async (query: string) => {
            try {
                let actualDir = (window as any).currentWorkspace || '.';
                if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
                else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);

                if ((window as any).vruttiSearch) {
                    const req = { directory: actualDir };
                    const resStr = await (window as any).vruttiSearch({ command: "find_files", ...req });
                    let files = JSON.parse(resStr) as string[];
                    
                    const q = query.toLowerCase();
                    if (q) {
                        files = files.filter(f => {
                            const lowerF = f.toLowerCase();
                            if (lowerF.includes(q)) return true;
                            let qIndex = 0;
                            for (let i = 0; i < lowerF.length; i++) {
                                if (lowerF[i] === q[qIndex]) {
                                    qIndex++;
                                    if (qIndex === q.length) return true;
                                }
                            }
                            return false;
                        });
                    }
                    return files.slice(0, 50).map(file => ({
                        label: file.split(/[\\/]/).pop() || file,
                        detail: file,
                        data: { type: 'file', path: file }
                    }));
                }
            } catch (e) {
                console.error("Failed to fetch files:", e);
            }
            return [];
        },
        onSelect: (item) => {
            if (item.data && item.data.type === 'file') {
                let fullPath = item.data.path;
                if (!fullPath.startsWith("file:///")) {
                    fullPath = "file:///" + fullPath;
                }
                window.dispatchEvent(new CustomEvent('open-file', {
                    detail: { path: fullPath, name: fullPath.split(/[\\/]/).pop(), line: 1 },
                }));
            }
        }
    });

    registry.registerQuickPickProvider({
        prefix: '>',
        description: 'Run a command...',
        getResults: (query: string) => {
            const commands = registry.getCommands();
            const q = query.toLowerCase();
            return commands.map(c => ({
                label: c.id,
                data: { type: 'command', id: c.id }
            })).filter(item => item.label.toLowerCase().includes(q));
        },
        onSelect: (item) => {
            if (item.data && item.data.type === 'command') {
                registry.executeCommand(item.data.id);
            }
        }
    });

    registry.registerQuickPickProvider({
        prefix: 'task ',
        description: 'Search tasks to run...',
        getResults: async (query: string) => {
            const config = await VruttiTaskManager.getTasksConfig();
            const tasks = config?.tasks || [];
            const q = query.toLowerCase();
            return tasks.filter(t => t.label.toLowerCase().includes(q) || t.command.toLowerCase().includes(q)).map(t => ({
                label: t.label + (t.isDefaultBuild ? ' (Default Build)' : ''),
                detail: t.command,
                data: { type: 'task_run', task: t }
            }));
        },
        onSelect: (item) => {
            if (item.data && item.data.type === 'task_run') {
                VruttiTaskManager.runTask(item.data.task);
            }
        }
    });

    registry.registerQuickPickProvider({
        prefix: 'task default ',
        description: 'Select default build task...',
        getResults: async (query: string) => {
            const config = await VruttiTaskManager.getTasksConfig();
            const tasks = config?.tasks || [];
            const q = query.toLowerCase();
            return tasks.filter(t => t.label.toLowerCase().includes(q) || t.command.toLowerCase().includes(q)).map(t => ({
                label: t.label + (t.isDefaultBuild ? ' (Default Build)' : ''),
                detail: t.command,
                data: { type: 'task_default', task: t }
            }));
        },
        onSelect: async (item) => {
            if (item.data && item.data.type === 'task_default') {
                try {
                    const config = await VruttiTaskManager.getTasksConfig();
                    if (config) {
                        config.tasks.forEach(t => {
                            t.isDefaultBuild = (t.label === item.data.task.label);
                        });
                        await VruttiTaskManager.saveTasksConfig(config);
                        const path = await VruttiTaskManager.ensureTasksFileExists();
                        if (path) {
                            window.dispatchEvent(new CustomEvent('open-file', {
                                detail: { path: path, name: 'tasks.json', line: 1 }
                            }));
                        }
                    }
                } catch (err) {
                    console.error('Failed to configure default build task:', err);
                }
            }
        }
    });

    // --- Dynamic Status Bar Items ---
    registry.registerCommand('vrutti.statusbar.updateLineCol', (args) => {
        window.dispatchEvent(new CustomEvent('vrutti-statusbar-update', {
            detail: {
                id: 'cursor-pos',
                text: `Ln ${args.line}, Col ${args.col}`
            }
        }));
    });

    registry.registerCommand('explorer.openFile', (context) => {
        if (context && context.path) {
            window.dispatchEvent(new CustomEvent('open-file', {
                detail: { path: context.path, name: context.name || '' }
            }));
        }
    });

    registry.registerCommand('explorer.rename', async (context) => {
        if (!context || !context.path || !context.name) return;
        
        const newName = window.prompt(`Rename '${context.name}' to:`, context.name);
        if (newName && newName !== context.name) {
            let basePath = context.path.substring(0, context.path.lastIndexOf('/'));
            if (!basePath.includes('/')) basePath = context.path.substring(0, context.path.lastIndexOf('\\'));
            const newPath = basePath + '/' + newName;
            
            let oldPathClean = context.path;
            if (oldPathClean.startsWith('file:///')) oldPathClean = oldPathClean.substring(8);
            else if (oldPathClean.startsWith('file://')) oldPathClean = oldPathClean.substring(7);

            let newPathClean = newPath;
            if (newPathClean.startsWith('file:///')) newPathClean = newPathClean.substring(8);
            else if (newPathClean.startsWith('file://')) newPathClean = newPathClean.substring(7);
            
            if ((window as any).vruttiRenameFile) {
                await (window as any).vruttiRenameFile(oldPathClean, newPathClean);
                window.dispatchEvent(new CustomEvent('explorer-refresh'));
            } else {
                console.warn('vruttiRenameFile not available in native backend');
            }
        }
    });

    registry.registerCommand('explorer.delete', async (context) => {
        if (!context || !context.path || !context.name) return;
        
        const confirmStr = window.confirm(`Are you sure you want to delete '${context.name}'?`);
        if (confirmStr) {
            let pathClean = context.path;
            if (pathClean.startsWith('file:///')) pathClean = pathClean.substring(8);
            else if (pathClean.startsWith('file://')) pathClean = pathClean.substring(7);
            
            if ((window as any).vruttiDeleteFile) {
                await (window as any).vruttiDeleteFile(pathClean);
                window.dispatchEvent(new CustomEvent('explorer-refresh'));
            } else {
                console.warn('vruttiDeleteFile not available in native backend');
            }
        }
    });

    registry.registerQuickPickProvider({
        prefix: 'task active ',
        description: 'Active tasks...',
        getResults: async (_query: string) => {
            return [{
                label: 'Running Terminal 1',
                detail: '(Active)',
                data: { type: 'task_active' }
            }];
        },
        onSelect: (item) => {
            if (item.data && item.data.type === 'task_active') {
                window.alert('Viewing active task logs in Terminal');
            }
        }
    });

    
    // Editor Types
    registry.registerEditorProvider({
        id: 'default',
        extensions: ['*'],
        component: 'vrutti-editor'
    });

    // SCM Commands
    registry.registerCommand('scm.viewGraph', () => {
        window.dispatchEvent(new CustomEvent('scm-view-graph'));
    });
    registry.registerCommand('scm.refresh', () => {
        window.dispatchEvent(new CustomEvent('scm-refresh'));
    });
    registry.registerCommand('scm.pull', () => {
        window.dispatchEvent(new CustomEvent('scm-pull'));
    });
    registry.registerCommand('scm.push', () => {
        window.dispatchEvent(new CustomEvent('scm-push'));
    });
    registry.registerCommand('scm.commit', () => {
        window.dispatchEvent(new CustomEvent('scm-commit'));
    });
    registry.registerCommand('scm.stageAll', () => {
        window.dispatchEvent(new CustomEvent('scm-stage-all'));
    });
    registry.registerCommand('scm.unstageAll', () => {
        window.dispatchEvent(new CustomEvent('scm-unstage-all'));
    });
    registry.registerCommand('scm.stageFile', (context) => {
        window.dispatchEvent(new CustomEvent('scm-stage-file', { detail: context }));
    });
    registry.registerCommand('scm.unstageFile', (context) => {
        window.dispatchEvent(new CustomEvent('scm-unstage-file', { detail: context }));
    });

    // Search Commands
    registry.registerCommand('search.toggleMatchCase', () => {
        window.dispatchEvent(new CustomEvent('search-toggle-match-case'));
    });
    registry.registerCommand('search.toggleWholeWord', () => {
        window.dispatchEvent(new CustomEvent('search-toggle-whole-word'));
    });
    registry.registerCommand('search.toggleRegex', () => {
        window.dispatchEvent(new CustomEvent('search-toggle-regex'));
    });
    registry.registerCommand('search.replaceAll', () => {
        window.dispatchEvent(new CustomEvent('search-replace-all'));
    });

    registry.registerMenu({
        id: 'search/inputActions',
        items: [
            { label: 'Match Case (Alt+C)', command: 'search.toggleMatchCase', order: 10, iconContent: 'Aa' },
            { label: 'Match Whole Word (Alt+W)', command: 'search.toggleWholeWord', order: 20, iconContent: 'ab' },
            { label: 'Use Regular Expression (Alt+R)', command: 'search.toggleRegex', order: 30, iconContent: '.*' }
        ]
    });

    registry.registerMenu({
        id: 'search/replaceActions',
        items: [
            { label: 'Replace All (Ctrl+Alt+Enter)', command: 'search.replaceAll', order: 10, iconContent: icon_replace_all }
        ]
    });
}
