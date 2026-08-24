export type ContributionType = 'activitybar' | 'views' | 'panel' | 'statusbar' | 'menus';

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
    title: string;
    order?: number;
    items: MenuItemContribution[];
}

export interface MenuItemContribution {
    id?: string;
    label: string;
    action?: string;
    separator?: boolean;
    order?: number;
}

class ContributionRegistry extends EventTarget {
    private activitybar: Map<string, ActivityBarContribution> = new Map();
    private views: Map<string, ViewContribution[]> = new Map(); // Key is containerId
    private panel: Map<string, PanelContribution> = new Map();
    private statusbar: Map<string, StatusBarContribution> = new Map();
    private menus: Map<string, MenuContribution> = new Map();

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

    // Getters
    getActivityBarItems(): ActivityBarContribution[] {
        return Array.from(this.activitybar.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    getViews(containerId: string): ViewContribution[] {
        return this.views.get(containerId) || [];
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
        return Array.from(this.menus.values()).sort((a, b) => (a.order || 0) - (b.order || 0));
    }

    private emitChange(type: ContributionType) {
        this.dispatchEvent(new CustomEvent('change', { detail: { type } }));
    }
}

export const registry = new ContributionRegistry();
