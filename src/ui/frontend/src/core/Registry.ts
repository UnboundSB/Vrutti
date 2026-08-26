export type ContributionType = 'activitybar' | 'views' | 'panel' | 'statusbar' | 'menus' | 'paneltabs' | 'configurations' | 'editors';

export interface CommandContribution {
    id: string;
    handler: (args?: any) => void;
}

export interface KeybindingContribution {
    key: string;
    command: string;
}

export interface PanelTabContribution {
    id: string;
    title: string;
    order?: number;
    component?: string;
}

export interface ActivityBarContribution {
    id: string;
    title: string;
    iconContent: string; // SVG content
    order?: number;
}

export interface ViewContribution {
    id: string;
    containerId: string;
    name: string;
    component: string; // The Lit component tag to render, e.g. 'vrutti-explorer'
}

export interface PanelContribution {
    id: string;
    title: string;
    order?: number;
    component?: string;
}

export interface StatusBarContribution {
    id: string;
    alignment: 'left' | 'right';
    order?: number;
    text?: string;
    iconContent?: string; // SVG content
    tooltip?: string;
    component?: string; // For dynamic custom rendering
}

export interface MenuContribution {
    id: string;
    title?: string;
    order?: number;
    items: MenuItemContribution[];
}

export interface MenuItemContribution {
    id?: string;
    label?: string; // Optional for separators
    action?: string;
    command?: string;
    separator?: boolean;
    order?: number;
    when?: string; // Context condition for visibility
    iconContent?: string; // SVG icon for action buttons
}

export interface SettingsCategoryContribution {
    id: string;
    title: string;
    component?: string; // Optional custom UI component instead of standard schema
    order?: number;
}

export interface ConfigurationSchema {
    id: string;
    title: string;
    order?: number;
    properties: Record<string, ConfigurationProperty>;
}

export interface ConfigurationProperty {
    type: 'string' | 'number' | 'boolean' | 'enum';
    default: any;
    description: string;
    enum?: string[];
}

export interface QuickPickItem {
    label: string;
    detail?: string;
    icon?: string;
    data?: any;
}

export interface QuickPickProvider {
    prefix: string;
    description: string;
    getResults: (query: string) => Promise<QuickPickItem[]> | QuickPickItem[];
    onSelect: (item: QuickPickItem) => void;
}

export interface EditorProvider {
    id: string;
    extensions: string[]; // e.g., ['.md', '.txt']
    component: string;
}

class ContributionRegistry extends EventTarget {
    private activitybar: Map<string, ActivityBarContribution> = new Map();
    private views: Map<string, ViewContribution[]> = new Map(); // Key is containerId
    private panel: Map<string, PanelContribution> = new Map();
    private panelTabs: PanelTabContribution[] = [];
    private statusbar: Map<string, StatusBarContribution> = new Map();
    private menus: Map<string, MenuContribution> = new Map();
    private commands: Map<string, CommandContribution> = new Map();
    private keybindings: Map<string, KeybindingContribution> = new Map();
    private configurations: Map<string, ConfigurationSchema> = new Map();
    private settingsCategories: Map<string, SettingsCategoryContribution> = new Map();
    private quickPickProviders: Map<string, QuickPickProvider> = new Map();
    private editors: Map<string, EditorProvider> = new Map();
    private outputChannels: Set<string> = new Set(['System']); // Default output channel

    registerActivityBar(item: ActivityBarContribution) {
        this.activitybar.set(item.id, item);
        this.emitChange('activitybar');
    }

    registerView(item: ViewContribution) {
        if (!this.views.has(item.containerId)) {
            this.views.set(item.containerId, []);
        }
        // avoid duplicates
        const existing = this.views.get(item.containerId)!;
        if (!existing.find(v => v.id === item.id)) {
            existing.push(item);
            this.emitChange('views');
        }
    }

    registerPanel(item: PanelContribution) {
        this.panel.set(item.id, item);
        this.emitChange('panel');
    }
    
    removePanel(id: string) {
        this.panel.delete(id);
        this.emitChange('panel');
    }

    registerStatusBar(item: StatusBarContribution) {
        this.statusbar.set(item.id, item);
        this.emitChange('statusbar');
    }

    registerMenu(item: MenuContribution) {
        this.menus.set(item.id, item);
        this.emitChange('menus');
    }

    registerCommand(id: string, handler: (args?: any) => void) {
        this.commands.set(id, { id, handler });
    }

    registerKeybinding(keybinding: KeybindingContribution) {
        this.keybindings.set(keybinding.key, keybinding);
    }

    executeCommand(id: string, args?: any) {
        const cmd = this.commands.get(id);
        if (cmd) {
            cmd.handler(args);
        } else {
            console.warn(`Command not found: ${id}`);
        }
    }

    registerConfiguration(schema: ConfigurationSchema) {
        this.configurations.set(schema.id, schema);
        this.emitChange('configurations');
    }

    registerSettingsCategory(category: SettingsCategoryContribution) {
        this.settingsCategories.set(category.id, category);
        this.emitChange('configurations');
    }

    registerOutputChannel(name: string) {
        this.outputChannels.add(name);
        this.emitChange('panel');
    }

    registerQuickPickProvider(provider: QuickPickProvider) {
        this.quickPickProviders.set(provider.prefix, provider);
    }

    registerEditorProvider(provider: EditorProvider) {
        this.editors.set(provider.id, provider);
        this.emitChange('editors');
    }

    getCommands(): CommandContribution[] {
        return Array.from(this.commands.values());
    }

    // Getters
    getActivityBarItems(): ActivityBarContribution[] {
        return Array.from(this.activitybar.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getViews(containerId: string): ViewContribution[] {
        return this.views.get(containerId) || [];
    }

    getSettingsCategories(): SettingsCategoryContribution[] {
        return Array.from(this.settingsCategories.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getOutputChannels(): string[] {
        return Array.from(this.outputChannels);
    }

    public registerPanelTab(tab: PanelTabContribution) {
        this.panelTabs.push(tab);
        this.panelTabs.sort((a, b) => (a.order || 0) - (b.order || 0));
        this.emitChange('paneltabs');
    }

    public removePanelTab(id: string) {
        this.panelTabs = this.panelTabs.filter(t => t.id !== id);
        this.emitChange('paneltabs');
    }

    getConfigurations(): ConfigurationSchema[] {
        return Array.from(this.configurations.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getQuickPickProviders(): QuickPickProvider[] {
        return Array.from(this.quickPickProviders.values());
    }

    getEditorProviderForExtension(extension: string): EditorProvider | undefined {
        for (const provider of this.editors.values()) {
            if (provider.extensions.includes(extension) || provider.extensions.includes('*')) {
                return provider;
            }
        }
        return undefined;
    }

    getPanelTabs(): PanelTabContribution[] {
        return this.panelTabs;
    }

    getPanelItems(): PanelContribution[] {
        return Array.from(this.panel.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getStatusBarItems(alignment: 'left' | 'right'): StatusBarContribution[] {
        return Array.from(this.statusbar.values())
            .filter(i => i.alignment === alignment)
            .sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getMenus(): MenuContribution[] {
        // Return only top-bar menus (those with titles)
        return Array.from(this.menus.values())
            .filter(m => m.title !== undefined)
            .sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getMenu(id: string): MenuContribution | undefined {
        return this.menus.get(id);
    }

    getKeybindings(): KeybindingContribution[] {
        return Array.from(this.keybindings.values());
    }

    private emitChange(type: ContributionType) {
        this.dispatchEvent(new CustomEvent('change', { detail: { type } }));
    }
}

export const registry = new ContributionRegistry();
