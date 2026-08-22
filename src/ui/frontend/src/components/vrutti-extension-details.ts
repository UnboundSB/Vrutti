import { LitElement, html, css } from 'lit';
import { customElement, property, state } from 'lit/decorators.js';

@customElement('vrutti-extension-details')
export class VruttiExtensionDetails extends LitElement {
    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background: var(--vrutti-bg, #1e1e1e);
            color: var(--vrutti-text, #cccccc);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            overflow-y: auto;
            padding: 24px;
            box-sizing: border-box;
        }

        .header {
            display: flex;
            align-items: flex-start;
            margin-bottom: 24px;
            position: relative;
        }

        .close-btn {
            position: absolute;
            top: 0;
            right: 0;
            background: transparent;
            border: none;
            color: var(--vrutti-text);
            cursor: pointer;
            padding: 8px;
            border-radius: 4px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        
        .close-btn:hover {
            background: var(--vrutti-surface-border, #3c3c3c);
            color: var(--vrutti-text-bright, #ffffff);
        }

        .ext-icon {
            width: 128px;
            height: 128px;
            margin-right: 24px;
            background: var(--vrutti-surface, #252526);
            border-radius: 8px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
        }

        .info {
            display: flex;
            flex-direction: column;
            justify-content: center;
        }

        .display-name {
            font-size: 28px;
            font-weight: 600;
            color: var(--vrutti-text-bright, #ffffff);
            margin-bottom: 8px;
        }

        .publisher {
            font-size: 14px;
            color: #007fd4;
            margin-bottom: 12px;
        }

        .description {
            font-size: 14px;
            color: var(--vrutti-text, #cccccc);
            margin-bottom: 16px;
            line-height: 1.5;
            max-width: 600px;
        }

        .install-btn {
            background: var(--vrutti-accent, #0e639c);
            color: #ffffff;
            border: none;
            padding: 8px 24px;
            border-radius: 2px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 600;
            align-self: flex-start;
            transition: background 0.2s;
        }

        .install-btn:hover {
            background: #1177bb;
        }

        .readme {
            border-top: 1px solid var(--vrutti-surface-border, #3c3c3c);
            padding-top: 24px;
            font-size: 14px;
            line-height: 1.6;
        }

        .progress-container {
            display: flex;
            align-items: center;
            margin-top: 12px;
            width: 100%;
            max-width: 300px;
        }

        .progress-bar-bg {
            flex: 1;
            height: 6px;
            background: #3c3c3c;
            border-radius: 3px;
            overflow: hidden;
            margin-right: 12px;
        }

        .progress-bar-fill {
            height: 100%;
            background: #007fd4;
            transition: width 0.1s linear;
        }

        .progress-text {
            font-size: 12px;
            color: #969696;
            min-width: 36px;
        }
    `;

    @property({ type: Object })
    set extension(val: any) {
        const oldVal = this._extension;
        if (oldVal?.name !== val?.name) {
            this.downloadProgress = null;
        }
        this._extension = val;
        this.requestUpdate('extension', oldVal);
    }
    get extension() {
        return this._extension;
    }
    private _extension: any = null;

    @state() private downloadProgress: number | null = null;

    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    }
    
    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    }

    private handleIpc = (e: CustomEvent) => {
        const msg = e.detail;
        if (msg && msg.method === 'extensions/progress' && this.extension && msg.params.name === this.extension.name) {
            this.downloadProgress = msg.params.percentage;
            if (this.downloadProgress === 100) {
                // Clear progress after a short delay
                setTimeout(() => { this.downloadProgress = null; }, 1500);
            }
        }
    };

    private install() {
        if (this.extension && (window as any).vruttiInstallExtension) {
            this.downloadProgress = 0; // Initialize progress to show UI immediately
            console.log(`Installing ${this.extension.name}...`);
            try {
                const res = (window as any).vruttiInstallExtension(this.extension.downloadUrl, this.extension.name);
                if (res && typeof res.catch === 'function') {
                    res.catch(console.error);
                }
            } catch (err) {
                console.error("Install error:", err);
            }
        }
    }

    render() {
        if (!this.extension) {
            return html`<div>No extension selected</div>`;
        }

        return html`
            <div class="header">
                <button class="close-btn" @click=${() => this.dispatchEvent(new CustomEvent('close-extension-details', { bubbles: true, composed: true }))} title="Close Extension Details">
                    <svg viewBox="0 0 16 16" width="20" height="20" fill="currentColor"><path d="M4.646 4.646a.5.5 0 0 1 .708 0L8 7.293l2.646-2.647a.5.5 0 0 1 .708.708L8.707 8l2.647 2.646a.5.5 0 0 1-.708.708L8 8.707l-2.646 2.647a.5.5 0 0 1-.708-.708L7.293 8 4.646 5.354a.5.5 0 0 1 0-.708z"/></svg>
                </button>
                <img class="ext-icon" src=${this.extension.iconUrl} @error=${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                <div class="info">
                    <div class="display-name">${this.extension.displayName}</div>
                    <div class="publisher">${this.extension.publisherDisplayName} | Version: ${this.extension.version}</div>
                    <div class="description">${this.extension.description}</div>
                    <button class="install-btn" @click=${this.install} ?disabled=${this.downloadProgress !== null || this.extension.isInstalled}>
                        ${this.extension.isInstalled ? 'Installed' : (this.downloadProgress !== null ? 'Installing...' : 'Install Extension')}
                    </button>
                    ${this.downloadProgress !== null ? html`
                        <div class="progress-container">
                            <div class="progress-bar-bg">
                                <div class="progress-bar-fill" style="width: ${this.downloadProgress}%"></div>
                            </div>
                            <span class="progress-text">${this.downloadProgress}%</span>
                        </div>
                    ` : ''}
                </div>
            </div>
            <div class="readme">
                <h3>About ${this.extension.displayName}</h3>
                <p>${this.extension.description}</p>
            </div>
        `;
    }
}
