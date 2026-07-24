import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface Config {
  'editor.fontSize': number;
  'editor.fontFamily': string;
  'editor.wordWrap': boolean;
  'workbench.colorTheme': string;
}

const DEFAULT_CONFIG: Config = {
  'editor.fontSize': 14,
  'editor.fontFamily': "'Fira Code', monospace",
  'editor.wordWrap': false,
  'workbench.colorTheme': 'Default Dark'
};

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-settings')
export class VruttiSettings extends LitElement {
  @state()
  private activeCategory = 'General';

  @state()
  private config: Config = { ...DEFAULT_CONFIG };

  private categories = ['General', 'Editor', 'Keybindings', 'Theme'];
  private saveTimeout: number | null = null;

  connectedCallback() {
    super.connectedCallback();
    // In a real implementation, we'd fetch settings from IPC here
    console.log('[Settings] Loaded lazy component');
  }

  private handleSettingChange(key: keyof Config, value: any) {
    this.config = { ...this.config, [key]: value };
    
    // Debounce the save event
    if (this.saveTimeout) {
      window.clearTimeout(this.saveTimeout);
    }
    
    this.saveTimeout = window.setTimeout(() => {
      this.dispatchEvent(new CustomEvent('setting-changed', {
        detail: { key, value: this.config[key] },
        bubbles: true,
        composed: true
      }));
      console.log(`[Settings] Saved ${key} = ${value}`);
    }, 500);
  }

  static styles = [globalHoverStyle, css`
    :host {
      display: flex;
      flex-direction: column;
      width: 100%;
      height: 100%;
      background-color: var(--vrutti-bg, #0f111a);
      color: var(--vrutti-text, #a6accd);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      z-index: 10;
    }

    .header {
      padding: 20px 24px 12px;
      font-size: 24px;
      font-weight: 300;
      color: var(--vrutti-text-bright, #fff);
      border-bottom: 1px solid var(--vrutti-surface-border, #23273b);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }

    .close-btn {
      background: none;
      border: none;
      color: var(--vrutti-text);
      cursor: pointer;
      font-size: 16px;
      padding: 4px 8px;
      border-radius: 4px;
    }

    .close-btn:hover {
      background: var(--vrutti-surface-border);
      color: var(--vrutti-text-bright);
    }

    .layout {
      display: flex;
      flex: 1;
      overflow: hidden;
    }

    .sidebar {
      width: 200px;
      padding: 16px 0;
      border-right: 1px solid var(--vrutti-surface-border, #23273b);
      display: flex;
      flex-direction: column;
    }

    .category-item {
      padding: 8px 24px;
      cursor: pointer;
      font-size: 13px;
      border-left: 2px solid transparent;
      transition: background 0.1s, color 0.1s;
    }

    .category-item:hover {
      background: var(--vrutti-surface, #13151f);
      color: var(--vrutti-text-bright, #fff);
    }

    .category-item.active {
      background: var(--vrutti-surface, #13151f);
      color: var(--vrutti-accent, #82aaff);
      border-left-color: var(--vrutti-accent, #82aaff);
      font-weight: 500;
    }

    .content {
      flex: 1;
      padding: 24px 40px;
      overflow-y: auto;
    }

    .setting-group {
      margin-bottom: 32px;
      max-width: 600px;
    }

    .setting-title {
      font-size: 14px;
      font-weight: 600;
      color: var(--vrutti-text-bright, #fff);
      margin-bottom: 8px;
    }

    .setting-desc {
      font-size: 12px;
      color: var(--vrutti-text, #a6accd);
      margin-bottom: 12px;
      opacity: 0.8;
    }

    .input-field {
      width: 100%;
      background: var(--vrutti-surface, #13151f);
      border: 1px solid var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #fff);
      padding: 8px 12px;
      border-radius: 4px;
      font-family: inherit;
      font-size: 13px;
    }
    
    .input-field:focus {
      outline: none;
      border-color: var(--vrutti-accent, #82aaff);
    }

    select.input-field {
      appearance: none;
      cursor: pointer;
    }

    .toggle-container {
      display: flex;
      align-items: center;
      gap: 12px;
    }

    .checkbox {
      width: 16px;
      height: 16px;
      cursor: pointer;
    }
  `;

  render() {
    return html`
      <div class="header">
        <span>Settings</span>
        <button class="close-btn" @click=${() => this.dispatchEvent(new CustomEvent('close-settings', { bubbles: true, composed: true }))}>✕</button>
      </div>
      
      <div class="layout">
        <div class="sidebar">
          ${this.categories.map(c => html`
            <div class="category-item ${this.activeCategory === c ? 'active' : ''}"
                 @click=${() => this.activeCategory = c}>
              ${c}
            </div>
          `)}
        </div>
        
        <div class="content">
          ${this.renderCategoryContent()}
        </div>
      </div>
    `;
  }

  private renderCategoryContent() {
    switch (this.activeCategory) {
      case 'Editor':
        return html`
          <div class="setting-group">
            <div class="setting-title">Font Size</div>
            <div class="setting-desc">Controls the font size in pixels.</div>
            <input type="number" class="input-field" .value=${this.config['editor.fontSize'].toString()} 
                   @input=${(e: any) => this.handleSettingChange('editor.fontSize', parseInt(e.target.value))} />
          </div>

          <div class="setting-group">
            <div class="setting-title">Font Family</div>
            <div class="setting-desc">Controls the font family.</div>
            <input type="text" class="input-field" .value=${this.config['editor.fontFamily']} 
                   @input=${(e: any) => this.handleSettingChange('editor.fontFamily', e.target.value)} />
          </div>

          <div class="setting-group">
            <div class="setting-title">Word Wrap</div>
            <div class="setting-desc">Controls how lines should wrap in the editor.</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['editor.wordWrap']} 
                     @change=${(e: any) => this.handleSettingChange('editor.wordWrap', e.target.checked)} />
              <span>Enable Word Wrap</span>
            </div>
          </div>
        `;
      case 'Theme':
        return html`
          <div class="setting-group">
            <div class="setting-title">Color Theme</div>
            <div class="setting-desc">Select the active color theme for the workspace.</div>
            <select class="input-field" .value=${this.config['workbench.colorTheme']}
                    @change=${(e: any) => this.handleSettingChange('workbench.colorTheme', e.target.value)}>
              <option value="Default Dark">Default Dark</option>
              <option value="One Dark Pro">One Dark Pro</option>
              <option value="Vrutti Glass">Vrutti Glass</option>
              <option value="Light+">Light+</option>
            </select>
          </div>
        `;
      case 'Keybindings':
      case 'General':
      default:
        return html`
          <div style="color: var(--vrutti-surface-border); margin-top: 40px; text-align: center;">
            No settings found in this category.
          </div>
        `;
    }
  }
}
