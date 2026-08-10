import { LitElement, html, css } from 'lit';
import { customElement, state, query } from 'lit/decorators.js';
import { globalHoverStyle } from '../shared-styles';
import { VruttiTaskManager, TaskDefinition } from './vrutti-task-manager';

@customElement('vrutti-task-picker')
export class VruttiTaskPicker extends LitElement {
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

        .task-name {
            font-weight: 500;
        }

        .task-command {
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
    @state() private tasks: TaskDefinition[] = [];
    @state() private filteredTasks: TaskDefinition[] = [];
    @state() private selectedIndex = 0;
    
    // mode can be 'run' or 'defaultBuild'
    public mode: 'run' | 'defaultBuild' | 'active' = 'run';

    @query('input') private inputEl!: HTMLInputElement;

    connectedCallback() {
        super.connectedCallback();
        this.fetchTasks();
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

    private async fetchTasks() {
        if (this.mode === 'active') {
            // We just list terminals here since we don't have a backend task tracker
            this.tasks = [{ label: 'Running Terminal 1', command: '(Active)' }];
        } else {
            const config = await VruttiTaskManager.getTasksConfig();
            if (config && config.tasks) {
                this.tasks = config.tasks;
            } else {
                this.tasks = [];
            }
        }
        this.filterTasks();
    }

    private filterTasks() {
        const q = this.query.toLowerCase();
        if (!q) {
            this.filteredTasks = this.tasks.slice();
        } else {
            this.filteredTasks = this.tasks.filter(t => 
                t.label.toLowerCase().includes(q) || t.command.toLowerCase().includes(q)
            );
        }
        this.selectedIndex = 0;
    }

    private handleInput(e: Event) {
        this.query = (e.target as HTMLInputElement).value;
        this.filterTasks();
    }

    private handleKeydown(e: KeyboardEvent) {
        if (e.key === 'ArrowDown') {
            e.preventDefault();
            this.selectedIndex = Math.min(this.selectedIndex + 1, this.filteredTasks.length - 1);
            this.scrollToSelected();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
            this.scrollToSelected();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (this.filteredTasks[this.selectedIndex]) {
                this.selectTask(this.filteredTasks[this.selectedIndex]);
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

    private async selectTask(task: TaskDefinition) {
        if (this.mode === 'run') {
            VruttiTaskManager.runTask(task);
        } else if (this.mode === 'defaultBuild') {
            const config = await VruttiTaskManager.getTasksConfig();
            if (config) {
                config.tasks.forEach(t => {
                    t.isDefaultBuild = (t.label === task.label);
                });
                await VruttiTaskManager.saveTasksConfig(config);
                // Also open the file to show it
                const path = await VruttiTaskManager.ensureTasksFileExists();
                if (path) {
                    this.dispatchEvent(new CustomEvent('open-file', {
                        detail: { path: path, name: 'tasks.json', line: 1 },
                        bubbles: true,
                        composed: true
                    }));
                }
            }
        } else if (this.mode === 'active') {
            window.alert('Viewing active task logs in Terminal');
        }
        this.close();
    }

    private close() {
        this.dispatchEvent(new CustomEvent('close-task-picker', { bubbles: true, composed: true }));
    }

    render() {
        return html`
            <div class="overlay" @mousedown=${(e: MouseEvent) => { if (e.target === e.currentTarget) this.close(); }}>
                <div class="palette">
                    <div class="input-container">
                        <input 
                            type="text" 
                            placeholder=${this.mode === 'defaultBuild' ? "Select default build task..." : "Search tasks to run..."}
                            .value=${this.query}
                            @input=${this.handleInput}
                            @keydown=${this.handleKeydown}
                        />
                    </div>
                    <div class="results">
                        ${this.filteredTasks.length === 0 ? html`<div class="no-results">No tasks found. ${this.mode !== 'active' ? 'Configure tasks in .vrutti/tasks.json' : ''}</div>` : ''}
                        ${this.filteredTasks.map((task, i) => html`
                            <div class="result-item ${i === this.selectedIndex ? 'selected' : ''}" 
                                 @click=${() => this.selectTask(task)}
                                 @mouseenter=${() => this.selectedIndex = i}>
                                <span class="task-name">${task.label} ${task.isDefaultBuild ? '(Default Build)' : ''}</span>
                                <span class="task-command">${task.command}</span>
                            </div>
                        `)}
                    </div>
                </div>
            </div>
        `;
    }
}
