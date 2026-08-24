import { LitElement, html, css } from 'lit';
import { customElement, state, query, property } from 'lit/decorators.js';
import { globalHoverStyle } from '../shared-styles';
import { registry, QuickPickItem, QuickPickProvider } from '../core/Registry';

@customElement('vrutti-quick-pick')
export class VruttiQuickPick extends LitElement {
    static styles = [globalHoverStyle, css`
        :host {
            display: block;
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            z-index: 10000;
            pointer-events: none;
        }

        .overlay {
            position: absolute;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0, 0, 0, 0.4);
            pointer-events: auto;
            display: flex;
            justify-content: center;
            align-items: flex-start;
            padding-top: 10vh;
        }

        .palette {
            width: 600px;
            max-width: 90vw;
            background: var(--vrutti-bg-dark, #1e1e1e);
            border: 1px solid #3b4261;
            border-radius: 8px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
            display: flex;
            flex-direction: column;
            overflow: hidden;
            font-family: var(--vrutti-font, 'Inter', -apple-system, sans-serif);
        }

        .input-container {
            padding: 12px;
            border-bottom: 1px solid #3b4261;
        }

        input {
            width: 100%;
            background: #1a1b26;
            border: 1px solid #7aa2f7;
            border-radius: 4px;
            color: #c0caf5;
            padding: 8px 12px;
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-sizing: border-box;
        }

        .results {
            max-height: 400px;
            overflow-y: auto;
            padding: 4px 0;
        }

        .result-item {
            padding: 6px 16px;
            cursor: pointer;
            color: #c0caf5;
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .result-item.selected {
            background: rgba(122, 162, 247, 0.2);
        }

        .result-item:hover {
            background: #292e42;
        }

        .item-label {
            font-weight: 500;
        }

        .item-detail {
            color: #565f89;
            font-size: 11px;
            margin-left: auto;
        }

        .no-results {
            padding: 12px;
            color: #565f89;
            text-align: center;
        }
    `];

    @state() private query = '';
    @state() private items: QuickPickItem[] = [];
    @state() private selectedIndex = 0;
    
    @property({ type: String }) public initialPrefix = '';

    @query('input') private inputEl!: HTMLInputElement;

    private activeProvider: QuickPickProvider | null = null;
    private providers: QuickPickProvider[] = [];

    connectedCallback() {
        super.connectedCallback();
        this.providers = registry.getQuickPickProviders().sort((a, b) => b.prefix.length - a.prefix.length);
        this.query = this.initialPrefix;
        this.fetchItems();
        window.addEventListener('keydown', this.handleGlobalKeydown);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('keydown', this.handleGlobalKeydown);
    }

    protected firstUpdated() {
        setTimeout(() => this.inputEl?.focus(), 50);
    }

    private handleGlobalKeydown = (e: KeyboardEvent) => {
        if (e.key === 'Escape') {
            this.close();
        }
    };

    private async fetchItems() {
        this.activeProvider = this.providers.find(p => this.query.startsWith(p.prefix)) || null;
        if (this.activeProvider) {
            const strippedQuery = this.query.slice(this.activeProvider.prefix.length);
            const results = await this.activeProvider.getResults(strippedQuery);
            this.items = results;
        } else {
            this.items = [];
        }
        this.selectedIndex = 0;
    }

    private handleInput(e: Event) {
        this.query = (e.target as HTMLInputElement).value;
        this.fetchItems();
    }

    private handleKeydown(e: KeyboardEvent) {
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            this.selectedIndex = Math.min(this.selectedIndex + 1, this.items.length - 1);
            this.scrollToSelected();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
            this.scrollToSelected();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (this.items[this.selectedIndex]) {
                this.selectItem(this.items[this.selectedIndex]);
            }
        }
    }

    private scrollToSelected() {
        const resultsEl = this.shadowRoot?.querySelector('.results');
        const selectedEl = this.shadowRoot?.querySelector('.result-item.selected') as HTMLElement;
        if (resultsEl && selectedEl) {
            const containerRect = resultsEl.getBoundingClientRect();
            const itemRect = selectedEl.getBoundingClientRect();
            if (itemRect.bottom > containerRect.bottom) {
                resultsEl.scrollTop += itemRect.bottom - containerRect.bottom;
            } else if (itemRect.top < containerRect.top) {
                resultsEl.scrollTop -= containerRect.top - itemRect.top;
            }
        }
    }

    private selectItem(item: QuickPickItem) {
        if (this.activeProvider) {
            this.activeProvider.onSelect(item);
        }
        this.close();
    }

    private close() {
        this.dispatchEvent(new CustomEvent('close-quick-pick', { bubbles: true, composed: true }));
    }

    render() {
        const placeholder = this.activeProvider ? this.activeProvider.description : 'Type to search...';

        return html`
            <div class="overlay" @mousedown=${(e: MouseEvent) => { if (e.target === e.currentTarget) this.close(); }}>
                <div class="palette">
                    <div class="input-container">
                        <input 
                            type="text" 
                            placeholder=${placeholder}
                            .value=${this.query}
                            @input=${this.handleInput}
                            @keydown=${this.handleKeydown}
                        />
                    </div>
                    <div class="results">
                        ${this.items.length === 0 ? html`<div class="no-results">No matching results found.</div>` : ''}
                        ${this.items.map((item, i) => html`
                            <div class="result-item ${i === this.selectedIndex ? 'selected' : ''}" 
                                 @click=${() => this.selectItem(item)}
                                 @mouseenter=${() => this.selectedIndex = i}>
                                <span class="item-label">${item.label}</span>
                                <span class="item-detail">${item.detail || ''}</span>
                            </div>
                        `)}
                    </div>
                </div>
            </div>
        `;
    }
}
