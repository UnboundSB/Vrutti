import { LitElement, html, css, PropertyValues, TemplateResult } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';
import { classMap } from 'lit/directives/class-map.js';

interface TreeNode {
    handle: string;
    label: string;
    collapsibleState: number; // 0=None, 1=Collapsed, 2=Expanded
    contextValue?: string;
    iconPath?: any;
    command?: any;
    tooltip?: string;
    children?: TreeNode[];
    expanded?: boolean;
}

@customElement('vrutti-tree-view')
export class VruttiTreeView extends LitElement {
    @property({ type: String }) viewId = '';
    @state() private rootNodes: TreeNode[] = [];
    @state() private loading = false;
    @state() private selectedHandle: string | null = null;

    static styles = css`
        :host {
            display: block;
            width: 100%;
            height: 100%;
            overflow-y: auto;
            color: var(--v-foreground);
            font-size: 13px;
        }
        
        .tree-node {
            display: flex;
            flex-direction: column;
        }
        
        .tree-item {
            display: flex;
            align-items: center;
            padding: 2px 0;
            cursor: pointer;
            user-select: none;
            line-height: 22px;
        }
        
        .tree-item:hover {
            background-color: var(--v-list-hover-background);
            color: var(--v-list-hover-foreground);
        }
        
        .tree-item.selected {
            background-color: var(--v-list-active-selection-background);
            color: var(--v-list-active-selection-foreground);
        }
        
        .twistie {
            width: 16px;
            height: 16px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-right: 4px;
            color: var(--v-foreground);
            opacity: 0.8;
        }
        
        .twistie svg {
            width: 16px;
            height: 16px;
            fill: currentColor;
            transition: transform 0.1s ease-in-out;
        }
        
        .twistie.expanded svg {
            transform: rotate(90deg);
        }
        
        .icon {
            width: 16px;
            height: 16px;
            margin-right: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .label {
            flex: 1;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        
        .children {
            display: flex;
            flex-direction: column;
        }
    `;

    protected firstUpdated(_changedProperties: PropertyValues): void {
        super.firstUpdated(_changedProperties);
        this.fetchChildren(null);
    }

    private async fetchChildren(parentHandle: string | null) {
        if (!parentHandle) this.loading = true;
        
        try {
            // We need a way to send an IPC request and wait for the response.
            // main.ts adds a global sendIpcMessage that doesn't return a promise by default,
            // but we can add a simple request-response wrapper in our frontend IPC layer.
            
            // For now, we'll implement a basic Promise wrapper around sendIpcMessage
            const result = await this.sendIpcRequest('treeview/request', {
                viewId: this.viewId,
                method: 'getChildren',
                nodeHandle: parentHandle
            });
            
            if (result && Array.isArray(result)) {
                const nodes = result.map(n => ({
                    ...n,
                    expanded: n.collapsibleState === 2
                }));
                
                if (!parentHandle) {
                    this.rootNodes = nodes;
                    // Auto-fetch children for expanded nodes
                    for (const node of this.rootNodes) {
                        if (node.expanded) {
                            this.fetchChildren(node.handle);
                        }
                    }
                } else {
                    this.updateNodeChildren(this.rootNodes, parentHandle, nodes);
                }
            }
        } catch (err) {
            console.error('Failed to fetch tree children:', err);
        } finally {
            if (!parentHandle) this.loading = false;
            this.requestUpdate();
        }
    }
    
    private updateNodeChildren(nodes: TreeNode[], parentHandle: string, children: TreeNode[]): boolean {
        for (let i = 0; i < nodes.length; i++) {
            if (nodes[i].handle === parentHandle) {
                nodes[i].children = children;
                // Auto-fetch children for expanded children
                for (const child of children) {
                    if (child.expanded) {
                        this.fetchChildren(child.handle);
                    }
                }
                return true;
            }
            if (nodes[i].children) {
                if (this.updateNodeChildren(nodes[i].children!, parentHandle, children)) {
                    return true;
                }
            }
        }
        return false;
    }

    private async sendIpcRequest(method: string, params: any): Promise<any> {
        return new Promise((resolve, reject) => {
            const reqId = Date.now() + Math.random().toString(36).substring(2, 9);
            
            const handleResponse = (e: any) => {
                try {
                    const msg = JSON.parse(e.data);
                    if (msg.method === 'treeview/response' && msg.params && msg.params.reqId === reqId) {
                        window.removeEventListener('message', handleResponse);
                        if (msg.params.error) {
                            reject(new Error(msg.params.error));
                        } else {
                            resolve(msg.params.result);
                        }
                    }
                } catch (err) {}
            };
            
            window.addEventListener('message', handleResponse);
            
            if ((window as any).sendIpcMessage) {
                (window as any).sendIpcMessage(method, JSON.stringify({ reqId, ...params }));
            } else {
                reject(new Error("IPC bridge not ready"));
            }
        });
    }

    private handleNodeClick(node: TreeNode) {
        this.selectedHandle = node.handle;
        
        if (node.command) {
            if ((window as any).sendIpcMessage) {
                (window as any).sendIpcMessage('command/execute', JSON.stringify({ 
                    command: node.command.command,
                    arguments: node.command.arguments 
                }));
            }
        }
        
        if (node.collapsibleState !== 0) {
            this.toggleNode(node);
        }
        
        this.requestUpdate();
    }
    
    private toggleNode(node: TreeNode) {
        node.expanded = !node.expanded;
        if (node.expanded && !node.children) {
            this.fetchChildren(node.handle);
        } else {
            this.requestUpdate();
        }
    }

    private renderChevron() {
        return html`
            <svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
                <path d="M5.7 12.3l4.6-4.6-4.6-4.6-.7.7 3.9 3.9-3.9 3.9z"/>
            </svg>
        `;
    }

    private renderNode(node: TreeNode, depth: number = 0): TemplateResult {
        const hasChildren = node.collapsibleState !== 0; // 0 = None
        
        return html`
            <div class="tree-node">
                <div class=${classMap({
                        'tree-item': true,
                        'selected': this.selectedHandle === node.handle
                     })} 
                     style="padding-left: ${depth * 12}px"
                     @click=${(e: Event) => { e.stopPropagation(); this.handleNodeClick(node); }}>
                    
                    <div class=${classMap({
                            'twistie': true,
                            'expanded': !!node.expanded
                         })}
                         @click=${(e: Event) => { if (hasChildren) { e.stopPropagation(); this.toggleNode(node); } }}>
                        ${hasChildren ? this.renderChevron() : ''}
                    </div>
                    
                    ${node.iconPath ? html`<div class="icon"><img src="${node.iconPath}" style="width:16px;height:16px;" /></div>` : ''}
                    
                    <div class="label" title=${node.tooltip || node.label}>
                        ${node.label}
                    </div>
                </div>
                
                ${node.expanded && node.children ? html`
                    <div class="children">
                        ${node.children.map(child => this.renderNode(child, depth + 1))}
                    </div>
                ` : ''}
            </div>
        `;
    }

    render() {
        if (this.loading) {
            return html`<div>Loading...</div>`;
        }
        
        if (this.rootNodes.length === 0) {
            return html`<div style="padding: 10px; opacity: 0.6; font-style: italic;">No items found.</div>`;
        }
        
        return html`
            <div class="tree-container">
                ${this.rootNodes.map(node => this.renderNode(node))}
            </div>
        `;
    }
}
