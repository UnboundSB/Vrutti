import { registry } from './Registry';
import { VruttiTaskManager } from '../components/vrutti-task-manager';
import { 
    icon_files, 
    icon_search, 
    icon_source_control, 
    icon_debug_alt, 
    icon_extensions,
    icon_git_branch,
    icon_error,
    icon_warning
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
        component: 'vrutti-explorer-view' // We will handle custom rendering based on activeTab in vrutti-sidebar for native elements first
    });
    
    // Panel Contributions
    registry.registerPanel({ id: 'PROBLEMS', title: 'PROBLEMS', order: 10 });
    registry.registerPanel({ id: 'OUTPUT', title: 'OUTPUT', order: 20 });
    registry.registerPanel({ id: 'DEBUG CONSOLE', title: 'DEBUG CONSOLE', order: 30 });
    registry.registerPanel({ id: 'TERMINAL', title: 'TERMINAL', order: 40, component: 'vrutti-terminal' });
    registry.registerPanel({ id: 'PORTS', title: 'PORTS', order: 50 });

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
        text: 'Spaces: 4',
        tooltip: 'Select Indentation'
    });
    registry.registerStatusBar({
        id: 'encoding',
        alignment: 'right',
        order: 30,
        text: 'UTF-8',
        tooltip: 'Select Encoding'
    });
    registry.registerStatusBar({
        id: 'eol',
        alignment: 'right',
        order: 40,
        text: 'CRLF',
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
        order: 80,
        items: [
            { label: 'Welcome', command: 'vrutti.action.help.welcome' },
            { label: 'Show All Commands', command: 'vrutti.action.showCommands' },
            { label: 'Documentation', command: 'vrutti.action.help.documentation' },
            { label: 'Editor Playground', command: 'vrutti.action.help.playground' },
            { label: 'Release Notes', command: 'vrutti.action.help.releaseNotes' },
            { label: 'sep1', separator: true },
            { label: 'Keyboard Shortcuts Reference', command: 'vrutti.action.help.keyboardShortcuts' },
            { label: 'Video Tutorials', command: 'vrutti.action.help.videoTutorials' },
            { label: 'Tips and Tricks', command: 'vrutti.action.help.tipsAndTricks' },
            { label: 'sep2', separator: true },
            { label: 'Join Us on YouTube', command: 'vrutti.action.help.youtube' },
            { label: 'Search Feature Requests', command: 'vrutti.action.help.featureRequests' },
            { label: 'Report Issue', command: 'vrutti.action.help.reportIssue' },
            { label: 'sep3', separator: true },
            { label: 'View License', command: 'vrutti.action.help.license' },
            { label: 'Privacy Statement', command: 'vrutti.action.help.privacy' },
            { label: 'sep4', separator: true },
            { label: 'Toggle Developer Tools', command: 'vrutti.action.toggleDevTools' },
            { label: 'sep5', separator: true },
            { label: 'Check for Updates', command: 'vrutti.action.help.checkUpdates' },
            { label: 'About', command: 'vrutti.action.help.about' }
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

    // Configuration
    registry.registerConfiguration({
        id: 'general',
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
        id: 'editor',
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
        id: 'appearance',
        title: 'Appearance',
        order: 30,
        properties: {
            'appearance.transparencyEffects': { type: 'boolean', default: false, description: 'Enable window transparency effects.' }
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
            }
        }
    });

    registry.registerQuickPickProvider({
        prefix: 'task active ',
        description: 'Active tasks...',
        getResults: async (query: string) => {
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
}
