import { LitElement, html, css, TemplateResult } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import './vrutti-editor';
import { globalHoverStyle } from '../shared-styles';

type PaneId = string;

export type LayoutNode = SplitNode | LeafNode;

export interface SplitNode {
    type: 'split';
    id: PaneId;
    direction: 'horizontal' | 'vertical';
    ratio: number; // 0.0 to 1.0
    first: LayoutNode;
    second: LayoutNode;
}

export interface LeafNode {
    type: 'leaf';
    id: PaneId;
    tabs: string[]; // array of file paths
    activeTab: string | null;
}

function generateId(): string {
    return Math.random().toString(36).substring(2, 9);
}

@customElement('vrutti-editor-layout')
export class VruttiEditorLayout extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: flex;
            width: 100%;
            height: 100%;
            overflow: hidden;
            background: var(--vrutti-bg);
            color: var(--vrutti-text);
        }

        .split-container {
            display: flex;
            width: 100%;
            height: 100%;
            overflow: hidden;
        }

        .split-container.horizontal {
            flex-direction: row;
        }

        .split-container.vertical {
            flex-direction: column;
        }

        .split-pane {
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }

        .sash {
            background: var(--vrutti-surface-border, #2a2e42);
            z-index: 10;
            transition: background 0.2s;
        }

        .sash:hover, .sash.active {
            background: var(--vrutti-accent, #7aa2f7);
        }

        .horizontal > .sash {
            width: 4px;
            cursor: col-resize;
            height: 100%;
        }

        .vertical > .sash {
            height: 4px;
            cursor: row-resize;
            width: 100%;
        }

        .leaf-container {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background: #1e1e1e;
        }

        .tabs-header {
            display: flex;
            background: #1a1b26;
            height: 35px;
            border-bottom: 1px solid #2a2e42;
            align-items: center;
            overflow-x: auto;
            overflow-y: hidden;
        }

        .tabs-header::-webkit-scrollbar {
            display: none;
        }

        .tab {
            display: flex;
            align-items: center;
            height: 100%;
            padding: 0 12px;
            cursor: pointer;
            border-right: 1px solid #2a2e42;
            background: #1a1b26;
            color: #a9b1d6;
            font-size: 13px;
            user-select: none;
            min-width: 80px;
            max-width: 200px;
        }

        .tab:hover {
            background: #292e42;
        }

        .tab.active {
            background: #1e1e1e;
            color: #c0caf5;
            border-top: 2px solid #7aa2f7;
        }

        .tab-title {
            flex: 1;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .tab-close {
            margin-left: 8px;
            opacity: 0;
            width: 16px;
            height: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 4px;
        }

        .tab:hover .tab-close {
            opacity: 1;
        }

        .tab-close:hover {
            background: #f7768e;
            color: white;
        }

        .pane-actions {
            display: flex;
            margin-left: auto;
            padding: 0 8px;
        }

        .pane-action {
            background: none;
            border: none;
            color: #a9b1d6;
            cursor: pointer;
            padding: 4px;
            margin-left: 4px;
            border-radius: 4px;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .pane-action:hover {
            background: #3b4261;
            color: #c0caf5;
        }

        .editor-area {
            flex: 1;
            position: relative;
            overflow: hidden;
            display: flex;
        }

        .empty-pane {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #565f89;
            font-size: 14px;
            user-select: none;
        }
    `];

    @state()
    public rootNode: LayoutNode = {
        type: 'leaf',
        id: generateId(),
        tabs: [],
        activeTab: null
    };

    @state()
    private activePaneId: PaneId = this.rootNode.id;

    // Resizing state
    private resizingNodeId: string | null = null;
    private resizeStartPos = 0;
    private resizeStartRatio = 0.5;
    private resizeTotalSize = 0;

    constructor() {
        super();
        this.activePaneId = this.rootNode.id;
    }

    public openFile(filePath: string) {
        // Find active pane or first available leaf
        let targetLeaf = this.findNode(this.rootNode, this.activePaneId) as LeafNode;
        if (!targetLeaf || targetLeaf.type !== 'leaf') {
            targetLeaf = this.findFirstLeaf(this.rootNode)!;
            if (targetLeaf) this.activePaneId = targetLeaf.id;
        }
        
        if (targetLeaf) {
            const newTabs = targetLeaf.tabs.includes(filePath) 
                ? targetLeaf.tabs 
                : [...targetLeaf.tabs, filePath];
                
            this.replaceNode(targetLeaf.id, {
                ...targetLeaf,
                tabs: newTabs,
                activeTab: filePath
            });
            this.requestUpdate();
        }
    }

    private findNode(node: LayoutNode, id: string): LayoutNode | null {
        if (node.id === id) return node;
        if (node.type === 'split') {
            return this.findNode(node.first, id) || this.findNode(node.second, id);
        }
        return null;
    }

    private findFirstLeaf(node: LayoutNode): LeafNode | null {
        if (node.type === 'leaf') return node;
        return this.findFirstLeaf(node.first) || this.findFirstLeaf(node.second);
    }

    private getParent(node: LayoutNode, id: string): SplitNode | null {
        if (node.type === 'leaf') return null;
        if (node.first.id === id || node.second.id === id) return node;
        return this.getParent(node.first, id) || this.getParent(node.second, id);
    }

    private replaceNode(id: string, newNode: LayoutNode) {
        if (this.rootNode.id === id) {
            this.rootNode = newNode;
        } else {
            const parent = this.getParent(this.rootNode, id);
            if (parent) {
                if (parent.first.id === id) parent.first = newNode;
                else parent.second = newNode;
            }
        }
    }

    private splitPane(leafId: string, direction: 'horizontal' | 'vertical') {
        const leaf = this.findNode(this.rootNode, leafId) as LeafNode;
        if (!leaf) return;

        const newLeaf: LeafNode = {
            type: 'leaf',
            id: generateId(),
            tabs: [],
            activeTab: null
        };

        const split: SplitNode = {
            type: 'split',
            id: generateId(),
            direction,
            ratio: 0.5,
            first: leaf,
            second: newLeaf
        };

        this.replaceNode(leafId, split);
        this.activePaneId = newLeaf.id;
        this.requestUpdate();
    }

    private closePane(leafId: string) {
        if (this.rootNode.id === leafId) {
            // Cannot close root, just empty it
            (this.rootNode as LeafNode).tabs = [];
            (this.rootNode as LeafNode).activeTab = null;
            this.requestUpdate();
            return;
        }

        const parent = this.getParent(this.rootNode, leafId);
        if (parent) {
            const sibling = parent.first.id === leafId ? parent.second : parent.first;
            this.replaceNode(parent.id, sibling);
            
            // if active pane was deleted, activate first available leaf
            if (this.activePaneId === leafId) {
                const firstLeaf = this.findFirstLeaf(this.rootNode);
                if (firstLeaf) this.activePaneId = firstLeaf.id;
            }
            this.requestUpdate();
        }
    }

    private closeTab(leafId: string, filePath: string, e: Event) {
        e.stopPropagation();
        const leaf = this.findNode(this.rootNode, leafId) as LeafNode;
        if (!leaf) return;

        const newTabs = leaf.tabs.filter(t => t !== filePath);
        let newActive = leaf.activeTab;
        if (leaf.activeTab === filePath) {
            newActive = newTabs.length > 0 ? newTabs[newTabs.length - 1] : null;
        }
        
        // Auto-close pane if empty and not root
        if (newTabs.length === 0 && this.rootNode.id !== leafId) {
            this.closePane(leafId);
        } else {
            this.replaceNode(leafId, {
                ...leaf,
                tabs: newTabs,
                activeTab: newActive
            });
            this.requestUpdate();
        }
    }

    private activateTab(leafId: string, filePath: string) {
        const leaf = this.findNode(this.rootNode, leafId) as LeafNode;
        if (leaf) {
            this.replaceNode(leafId, {
                ...leaf,
                activeTab: filePath
            });
            this.activePaneId = leafId;
            this.requestUpdate();
        }
    }

    // --- Resizing logic ---

    private startResize = (e: MouseEvent, splitId: string, direction: 'horizontal' | 'vertical') => {
        this.resizingNodeId = splitId;
        const split = this.findNode(this.rootNode, splitId) as SplitNode;
        if (!split) return;

        this.resizeStartRatio = split.ratio;
        
        // Need parent container size
        const target = e.target as HTMLElement;
        const parent = target.parentElement as HTMLElement;
        this.resizeTotalSize = direction === 'horizontal' ? parent.clientWidth : parent.clientHeight;
        
        this.resizeStartPos = direction === 'horizontal' ? e.clientX : e.clientY;

        window.addEventListener('mousemove', this.doResize);
        window.addEventListener('mouseup', this.stopResize);
        document.body.style.cursor = direction === 'horizontal' ? 'col-resize' : 'row-resize';
    };

    private doResize = (e: MouseEvent) => {
        if (!this.resizingNodeId) return;
        const split = this.findNode(this.rootNode, this.resizingNodeId) as SplitNode;
        if (!split) return;

        const currentPos = split.direction === 'horizontal' ? e.clientX : e.clientY;
        const delta = currentPos - this.resizeStartPos;
        const deltaRatio = delta / this.resizeTotalSize;
        
        let newRatio = this.resizeStartRatio + deltaRatio;
        if (newRatio < 0.1) newRatio = 0.1;
        if (newRatio > 0.9) newRatio = 0.9;
        
        split.ratio = newRatio;
        this.requestUpdate();
    };

    private stopResize = () => {
        this.resizingNodeId = null;
        window.removeEventListener('mousemove', this.doResize);
        window.removeEventListener('mouseup', this.stopResize);
        document.body.style.cursor = '';
    };

    // --- Rendering ---

    private getBasename(path: string) {
        return path.split(/[\\/]/).pop() || path;
    }
    
    private onDragStart(e: DragEvent, sourceLeafId: string, filePath: string) {
        if (e.dataTransfer) {
            e.dataTransfer.setData('application/json', JSON.stringify({ sourceLeafId, filePath }));
            e.dataTransfer.effectAllowed = 'move';
        }
    }

    private onDrop(e: DragEvent, targetLeafId: string) {
        e.preventDefault();
        if (e.dataTransfer) {
            const dataStr = e.dataTransfer.getData('application/json');
            if (dataStr) {
                try {
                    const data = JSON.parse(dataStr);
                    if (data.sourceLeafId && data.filePath) {
                        this.moveTab(data.sourceLeafId, targetLeafId, data.filePath);
                    }
                } catch (e) {}
            }
        }
    }

    private moveTab(sourceLeafId: string, targetLeafId: string, filePath: string) {
        if (sourceLeafId === targetLeafId) return;

        const sourceLeaf = this.findNode(this.rootNode, sourceLeafId) as LeafNode;
        const targetLeaf = this.findNode(this.rootNode, targetLeafId) as LeafNode;
        if (!sourceLeaf || !targetLeaf) return;

        const newSourceTabs = sourceLeaf.tabs.filter(t => t !== filePath);
        let newSourceActive = sourceLeaf.activeTab;
        if (sourceLeaf.activeTab === filePath) {
            newSourceActive = newSourceTabs.length > 0 ? newSourceTabs[newSourceTabs.length - 1] : null;
        }

        const newTargetTabs = targetLeaf.tabs.includes(filePath) ? targetLeaf.tabs : [...targetLeaf.tabs, filePath];
        
        this.replaceNode(sourceLeafId, {
            ...sourceLeaf,
            tabs: newSourceTabs,
            activeTab: newSourceActive
        });

        this.replaceNode(targetLeafId, {
            ...targetLeaf,
            tabs: newTargetTabs,
            activeTab: filePath
        });

        this.activePaneId = targetLeafId;

        if (newSourceTabs.length === 0 && this.rootNode.id !== sourceLeafId) {
            this.closePane(sourceLeafId);
        } else {
            this.requestUpdate();
        }
    }

    private renderLeaf(leaf: LeafNode): TemplateResult {
        return html`
            <div class="leaf-container" @click=${() => { this.activePaneId = leaf.id; this.requestUpdate(); }}>
                <div class="tabs-header" @dragover=${(e: DragEvent) => e.preventDefault()} @drop=${(e: DragEvent) => this.onDrop(e, leaf.id)}>
                    ${leaf.tabs.map(tab => html`
                        <div class="tab ${leaf.activeTab === tab ? 'active' : ''}" 
                             draggable="true" 
                             @dragstart=${(e: DragEvent) => this.onDragStart(e, leaf.id, tab)}
                             @click=${() => this.activateTab(leaf.id, tab)}>
                            <span class="tab-title" title="${tab}">${this.getBasename(tab)}</span>
                            <span class="tab-close" @click=${(e: Event) => this.closeTab(leaf.id, tab, e)}>
                                <svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor"><path d="M4.28 3.22a.75.75 0 0 0-1.06 1.06L6.94 8l-3.72 3.72a.75.75 0 1 0 1.06 1.06L8 9.06l3.72 3.72a.75.75 0 1 0 1.06-1.06L9.06 8l3.72-3.72a.75.75 0 0 0-1.06-1.06L8 6.94 4.28 3.22z"/></svg>
                            </span>
                        </div>
                    `)}
                    <div class="pane-actions">
                        <button class="pane-action" title="Split Right" @click=${() => this.splitPane(leaf.id, 'horizontal')}>
                            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2 2h12v12H2V2zm5 11h6V3H7v10zM3 13h3V3H3v10z"/></svg>
                        </button>
                        <button class="pane-action" title="Split Down" @click=${() => this.splitPane(leaf.id, 'vertical')}>
                            <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M2 2h12v12H2V2zm11 5V3H3v4h10zM13 8H3v5h10V8z"/></svg>
                        </button>
                        ${this.rootNode.id !== leaf.id ? html`
                            <button class="pane-action" title="Close Pane" @click=${() => this.closePane(leaf.id)}>
                                <svg viewBox="0 0 16 16" width="14" height="14" fill="currentColor"><path d="M4.28 3.22a.75.75 0 0 0-1.06 1.06L6.94 8l-3.72 3.72a.75.75 0 1 0 1.06 1.06L8 9.06l3.72 3.72a.75.75 0 1 0 1.06-1.06L9.06 8l3.72-3.72a.75.75 0 0 0-1.06-1.06L8 6.94 4.28 3.22z"/></svg>
                            </button>
                        ` : ''}
                    </div>
                </div>
                <div class="editor-area ${this.activePaneId === leaf.id ? 'active-pane' : ''}" style="${this.activePaneId === leaf.id ? 'box-shadow: inset 0 0 0 1px #3b4261;' : ''}">
                    ${leaf.activeTab ? html`
                        <vrutti-editor .filePath=${leaf.activeTab} style="width: 100%; height: 100%;"></vrutti-editor>
                    ` : html`
                        <div class="empty-pane" style="position: relative; width: 100%; height: 100%; overflow: hidden;">
                            <svg style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); opacity: 0.05; pointer-events: none; user-select: none; width: 300px; height: 300px;" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                                <path d="M12 2L2 22H6L12 10L18 22H22L12 2Z" fill="currentColor"/>
                            </svg>
                        </div>
                    `}
                </div>
            </div>
        `;
    }

    private renderNode(node: LayoutNode): TemplateResult {
        if (node.type === 'leaf') {
            return this.renderLeaf(node);
        } else {
            return html`
                <div class="split-container ${node.direction}">
                    <div class="split-pane" style="flex: ${node.ratio};">
                        ${this.renderNode(node.first)}
                    </div>
                    <div class="sash ${this.resizingNodeId === node.id ? 'active' : ''}" @mousedown=${(e: MouseEvent) => this.startResize(e, node.id, node.direction)}></div>
                    <div class="split-pane" style="flex: ${1 - node.ratio};">
                        ${this.renderNode(node.second)}
                    </div>
                </div>
            `;
        }
    }

    render() {
        return this.renderNode(this.rootNode);
    }
}
