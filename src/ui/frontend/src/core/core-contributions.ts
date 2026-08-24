import { registry } from './Registry';
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
            { label: 'New File', order: 10 },
            { label: 'New Window', order: 20 },
            { label: 'separator1', separator: true, order: 30 },
            { label: 'Open File', order: 40 },
            { label: 'Open Folder...', action: 'openFolder', order: 50 },
            { label: 'Open Recent', order: 60 },
            { label: 'separator2', separator: true, order: 70 },
            { label: 'Save', order: 80 },
            { label: 'Save As', order: 90 },
            { label: 'Save All', order: 100 },
            { label: 'separator3', separator: true, order: 110 },
            { label: 'Auto Save', order: 120 },
            { label: 'Preferences', order: 130 },
            { label: 'separator4', separator: true, order: 140 },
            { label: 'Close Editor', order: 150 },
            { label: 'Close Folder', order: 160 },
            { label: 'Close Window', order: 170 },
            { label: 'separator5', separator: true, order: 180 },
            { label: 'Exit', action: 'closeWindow', order: 190 }
        ]
    });

    // Menubar Contributions (Edit)
    registry.registerMenu({
        id: 'edit',
        title: 'Edit',
        order: 20,
        items: [
            { label: 'Undo', order: 10 },
            { label: 'Redo', order: 20 },
            { label: 'sep1', separator: true, order: 30 },
            { label: 'Cut', order: 40 },
            { label: 'Copy', order: 50 },
            { label: 'Paste', order: 60 },
            { label: 'sep2', separator: true, order: 70 },
            { label: 'Find', order: 80 },
            { label: 'Replace', order: 90 },
            { label: 'sep3', separator: true, order: 100 },
            { label: 'Find in Files', order: 110 },
            { label: 'Replace in Files', order: 120 },
            { label: 'sep4', separator: true, order: 130 },
            { label: 'Toggle Line Comment', order: 140 },
            { label: 'Toggle Block Comment', order: 150 }
        ]
    });

    // Selection Menu
    registry.registerMenu({
        id: 'selection',
        title: 'Selection',
        order: 30,
        items: [
            { label: 'Select All' },
            { label: 'Expand Selection' },
            { label: 'Shrink Selection' },
            { label: 'sep1', separator: true },
            { label: 'Copy Line Up' },
            { label: 'Copy Line Down' },
            { label: 'Move Line Up' },
            { label: 'Move Line Down' },
            { label: 'Duplicate Selection' },
            { label: 'sep2', separator: true },
            { label: 'Add Cursor Above' },
            { label: 'Add Cursor Below' },
            { label: 'Add Cursors to Line Ends' },
            { label: 'Add Next Occurrence' },
            { label: 'Add Previous Occurrence' },
            { label: 'Select All Occurrences' },
            { label: 'sep3', separator: true },
            { label: 'Switch to Ctrl+Click for Multi-Cursor' },
            { label: 'Column Selection Mode' }
        ]
    });

    // View Menu
    registry.registerMenu({
        id: 'view',
        title: 'View',
        order: 40,
        items: [
            { label: 'Command Palette' },
            { label: 'Open View' },
            { label: 'sep1', separator: true },
            { label: 'Appearance' },
            { label: 'Editor Layout' },
            { label: 'sep2', separator: true },
            { label: 'Explorer' },
            { label: 'Search' },
            { label: 'Source Control' },
            { label: 'Run' },
            { label: 'Extensions' },
            { label: 'sep3', separator: true },
            { label: 'Problems' },
            { label: 'Output' },
            { label: 'Debug Console' },
            { label: 'Terminal', action: 'toggleTerminal' },
            { label: 'sep4', separator: true },
            { label: 'Word Wrap' }
        ]
    });

    // Go Menu
    registry.registerMenu({
        id: 'go',
        title: 'Go',
        order: 50,
        items: [
            { label: 'Back' },
            { label: 'Forward' },
            { label: 'sep1', separator: true },
            { label: 'Go to File' },
            { label: 'Go to Symbol in Workspace' },
            { label: 'sep2', separator: true },
            { label: 'Go to Line/Column' },
            { label: 'Go to Definition' },
            { label: 'Go to Declaration' },
            { label: 'Go to Type Definition' },
            { label: 'Go to Implementations' },
            { label: 'Go to References' },
            { label: 'sep3', separator: true },
            { label: 'Next Problem' },
            { label: 'Previous Problem' },
            { label: 'Next Change' },
            { label: 'Previous Change' }
        ]
    });

    // Run Menu
    registry.registerMenu({
        id: 'run',
        title: 'Run',
        order: 60,
        items: [
            { label: 'Start Debugging' },
            { label: 'Run Without Debugging' },
            { label: 'Stop Debugging' },
            { label: 'Restart Debugging' },
            { label: 'sep1', separator: true },
            { label: 'Open Configurations' },
            { label: 'Add Configuration' },
            { label: 'sep2', separator: true },
            { label: 'Step Over' },
            { label: 'Step Into' },
            { label: 'Step Out' },
            { label: 'Continue' },
            { label: 'sep3', separator: true },
            { label: 'Toggle Breakpoint' },
            { label: 'New Breakpoint' }
        ]
    });

    // Terminal Menu
    registry.registerMenu({
        id: 'terminal',
        title: 'Terminal',
        order: 70,
        items: [
            { label: 'New Terminal', action: 'toggleTerminal' },
            { label: 'sep1', separator: true },
            { label: 'Run Task' },
            { label: 'Build Task' },
            { label: 'Active Tasks' },
            { label: 'sep2', separator: true },
            { label: 'Configure Tasks' },
            { label: 'Configure Default Build Task' }
        ]
    });

    // Help Menu
    registry.registerMenu({
        id: 'help',
        title: 'Help',
        order: 80,
        items: [
            { label: 'Welcome' },
            { label: 'Show All Commands' },
            { label: 'Documentation' },
            { label: 'Editor Playground' },
            { label: 'Release Notes' },
            { label: 'sep1', separator: true },
            { label: 'Keyboard Shortcuts Reference' },
            { label: 'Video Tutorials' },
            { label: 'Tips and Tricks' },
            { label: 'sep2', separator: true },
            { label: 'Join Us on YouTube' },
            { label: 'Search Feature Requests' },
            { label: 'Report Issue' },
            { label: 'sep3', separator: true },
            { label: 'View License' },
            { label: 'Privacy Statement' },
            { label: 'sep4', separator: true },
            { label: 'Toggle Developer Tools' },
            { label: 'sep5', separator: true },
            { label: 'Check for Updates' },
            { label: 'About' }
        ]
    });
}
