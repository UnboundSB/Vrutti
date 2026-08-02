import { LitElement, html, css } from 'lit';
import { customElement, property } from 'lit/decorators.js';

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
    `;

    @property({ type: Object }) extension: any = null;

    private install() {
        if (this.extension && (window as any).vruttiInstallExtension) {
            console.log(\`Installing \${this.extension.name}...\`);
            (window as any).vruttiInstallExtension(this.extension.downloadUrl, this.extension.name).catch(console.error);
        }
    }

    render() {
        if (!this.extension) {
            return html`<div>No extension selected</div>`;
        }

        return html`
            <div class="header">
                <img class="ext-icon" src=\${this.extension.iconUrl} @error=\${(e: Event) => (e.target as HTMLImageElement).style.display = 'none'} />
                <div class="info">
                    <div class="display-name">\${this.extension.displayName}</div>
                    <div class="publisher">\${this.extension.publisherDisplayName} | Version: \${this.extension.version}</div>
                    <div class="description">\${this.extension.description}</div>
                    <button class="install-btn" @click=\${this.install}>Install Extension</button>
                </div>
            </div>
            <div class="readme">
                <h3>Extension Overview</h3>
                <p>Welcome to \${this.extension.displayName}. Click the Install button above to download and apply this extension.</p>
                <p>Because Vrutti fetches directly from the Open VSX Registry, all downloaded extensions are fully open source.</p>
            </div>
        `;
    }
}
