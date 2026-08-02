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

    @property({ type: Object }) extension: any = null;
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
            (window as any).vruttiInstallExtension(this.extension.downloadUrl, this.extension.name).catch(console.error);
        }
    }

    render() {
        if (!this.extension) {
            return html`<div>No extension selected</div>`;
        }

        return html`
            <div class="header">
                <img class="ext-icon" src=${this.extension.iconUrl} @error=${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                <div class="info">
                    <div class="display-name">${this.extension.displayName}</div>
                    <div class="publisher">${this.extension.publisherDisplayName} | Version: ${this.extension.version}</div>
                    <div class="description">${this.extension.description}</div>
                    <button class="install-btn" @click=${this.install} ?disabled=${this.downloadProgress !== null}>
                        ${this.downloadProgress !== null ? 'Installing...' : 'Install Extension'}
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
