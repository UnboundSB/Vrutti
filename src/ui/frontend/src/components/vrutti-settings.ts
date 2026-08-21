import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';

interface Config {
  'editor.fontSize': number;
  'editor.fontFamily': string;
  'editor.wordWrap': boolean;
  'editor.tabSize': number;
  'editor.insertSpaces': boolean;
  'editor.minimap.enabled': boolean;
  'files.autoSave': boolean;
  'telemetry.enableTelemetry': boolean;
  'appearance.transparencyEffects': boolean;
  'workbench.colorTheme'?: string;
}

const DEFAULT_CONFIG: Config = {
  'editor.fontSize': 14,
  'editor.fontFamily': "'Fira Code', monospace",
  'editor.wordWrap': false,
  'editor.tabSize': 4,
  'editor.insertSpaces': true,
  'editor.minimap.enabled': true,
  'files.autoSave': false,
  'telemetry.enableTelemetry': false,
  'appearance.transparencyEffects': false
};

import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-settings')
export class VruttiSettings extends LitElement {
  @state()
  private activeCategory = 'General';

  @state()
  private config: Config = { ...DEFAULT_CONFIG };

  @state()
  private availableThemes: { id: string, label: string }[] = [];

  @state()
  private appliedTheme: string = '';

  @state()
  private selectedTheme: string = '';

  @state()
  private showDirtyModal = false;

  private categories = ['General', 'Editor', 'Keybindings', 'Theme', 'Appearance'];
  private saveTimeout: number | null = null;

  connectedCallback() {
    super.connectedCallback();
    console.log('[Settings] Loaded lazy component');
    
    // Load persisted theme state
    try {
      const applied = localStorage.getItem('vrutti-applied-theme');
      if (applied) {
        const t = JSON.parse(applied);
        this.appliedTheme = t.id || t.name;
        this.selectedTheme = this.appliedTheme;
      }
    } catch (e) {}

    window.addEventListener('vrutti-ipc', this.handleIpc as EventListener);
    // Request themes
    if ((window as any).sendIpcMessage) {
      (window as any).sendIpcMessage('extensions/request_installed', '{}');
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    if (this.saveTimeout) {
      window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
  }

  private handleIpc = (e: Event) => {
    const msg = (e as CustomEvent).detail;
    if (msg && msg.method === 'themes/available' && msg.params) {
      this.availableThemes = msg.params;
    }
  };

  private requestClose() {
    if (this.selectedTheme !== this.appliedTheme) {
      this.showDirtyModal = true;
      return;
    }
    this.dispatchEvent(new CustomEvent('close-settings', { bubbles: true, composed: true }));
  }

  private confirmExit() {
    this.selectedTheme = this.appliedTheme;
    this.showDirtyModal = false;
    this.dispatchEvent(new CustomEvent('close-settings', { bubbles: true, composed: true }));
  }

  private cancelExit() {
    this.showDirtyModal = false;
  }

  private apply() {
    this.appliedTheme = this.selectedTheme;
    // Broadcast setting-changed so it gets saved to backend and triggers ThemeBridge
    this.handleSettingChange('workbench.colorTheme' as keyof Config, this.selectedTheme);
  }

  private applyAndExit() {
    this.apply();
    this.dispatchEvent(new CustomEvent('close-settings', { bubbles: true, composed: true }));
  }

  private handleSettingChange(key: keyof Config, value: any) {
    this.config = { ...this.config, [key]: value };
    
    if (key === 'workbench.colorTheme') {
      this.appliedTheme = value;
      this.selectedTheme = value;
    }

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
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }

    .settings-scroll {
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

    .footer-actions {
      display: flex;
      justify-content: flex-end;
      gap: 12px;
      padding: 16px 24px;
      background: var(--vrutti-surface, #13151f);
      border-top: 1px solid var(--vrutti-surface-border, #23273b);
    }

    .btn {
      background: var(--vrutti-surface-border, #23273b);
      border: 1px solid transparent;
      color: var(--vrutti-text-bright, #fff);
      cursor: pointer;
      padding: 6px 16px;
      border-radius: 4px;
      font-size: 13px;
    }

    .btn:hover {
      background: rgba(255, 255, 255, 0.1);
    }

    .btn-primary {
      background: var(--vrutti-accent, #0e639c);
    }
    .btn-primary:hover {
      filter: brightness(1.2);
      background: var(--vrutti-accent, #0e639c);
    }

    .modal-overlay {
      position: absolute;
      top: 0; left: 0; right: 0; bottom: 0;
      background: rgba(0, 0, 0, 0.5);
      display: flex;
      justify-content: center;
      align-items: center;
      z-index: 100;
    }

    .modal-content {
      background: var(--vrutti-surface, #13151f);
      border: 1px solid var(--vrutti-surface-border, #23273b);
      border-radius: 6px;
      padding: 24px;
      width: 400px;
      max-width: 90%;
      box-shadow: 0 4px 20px rgba(0,0,0,0.5);
    }

    .modal-title {
      font-size: 16px;
      font-weight: 600;
      margin-bottom: 12px;
    }

    .modal-body {
      font-size: 13px;
      color: var(--vrutti-text);
      margin-bottom: 24px;
    }

    .modal-actions {
      display: flex;
      justify-content: flex-end;
      gap: 12px;
    }
  `];

  render() {
    return html`
      ${this.showDirtyModal ? html`
        <div class="modal-overlay">
          <div class="modal-content">
            <div class="modal-title">Unapplied Changes</div>
            <div class="modal-body">Do you want to exit without applying your changes?</div>
            <div class="modal-actions">
              <button class="btn" @click=${this.cancelExit}>Cancel</button>
              <button class="btn btn-primary" @click=${this.confirmExit}>Exit without Applying</button>
            </div>
          </div>
        </div>
      ` : ''}

      <div class="header">
        <span>Settings</span>
        <button class="close-btn" @click=${this.requestClose}>✕</button>
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
          <div class="settings-scroll">
            ${this.renderCategoryContent()}
          </div>
          <div class="footer-actions">
            <button class="btn" @click=${this.requestClose}>Cancel</button>
            <button class="btn" @click=${this.applyAndExit}>OK</button>
            <button class="btn btn-primary" @click=${this.apply}>Apply</button>
          </div>
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

          <div class="setting-group">
            <div class="setting-title">Tab Size</div>
            <div class="setting-desc">The number of spaces a tab is equal to.</div>
            <input type="number" class="input-field" .value=${this.config['editor.tabSize'].toString()} 
                   @input=${(e: any) => this.handleSettingChange('editor.tabSize', parseInt(e.target.value))} />
          </div>

          <div class="setting-group">
            <div class="setting-title">Insert Spaces</div>
            <div class="setting-desc">Insert spaces when pressing Tab.</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['editor.insertSpaces']} 
                     @change=${(e: any) => this.handleSettingChange('editor.insertSpaces', e.target.checked)} />
              <span>Use Spaces for Indentation</span>
            </div>
          </div>

          <div class="setting-group">
            <div class="setting-title">Minimap</div>
            <div class="setting-desc">Controls whether the minimap is shown.</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['editor.minimap.enabled']} 
                     @change=${(e: any) => this.handleSettingChange('editor.minimap.enabled', e.target.checked)} />
              <span>Enable Minimap</span>
            </div>
          </div>
        `;
      case 'Theme':
        return html`
          <div class="setting-group">
            <div class="setting-title">Color Theme</div>
            <div class="setting-desc">Select the active color theme for the workspace. Applies upon clicking Apply and Exit.</div>
            <select class="input-field" @change=${(e: any) => this.selectedTheme = e.target.value}>
              ${this.availableThemes.length === 0 ? html`<option value="${this.selectedTheme}" selected>${this.selectedTheme}</option>` : ''}
              ${this.availableThemes.map(t => html`<option value="${t.id}" ?selected=${t.id === this.selectedTheme}>${t.label}</option>`)}
            </select>
          </div>
        `;
      case 'Appearance':
        return html`
          <div class="setting-group">
            <div class="setting-title">Enable Transparency Effects</div>
            <div class="setting-desc">Apply premium frosted glass transparency to UI panels. (Recommended with Vrutti Glass theme)</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['appearance.transparencyEffects']} 
                     @change=${(e: any) => this.handleSettingChange('appearance.transparencyEffects', e.target.checked)} />
              <span>Transparency Effects Enabled</span>
            </div>
          </div>
        `;
      case 'General':
        return html`
          <div class="setting-group">
            <div class="setting-title">Auto Save</div>
            <div class="setting-desc">Controls whether dirty files are automatically saved.</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['files.autoSave']} 
                     @change=${(e: any) => this.handleSettingChange('files.autoSave', e.target.checked)} />
              <span>Enable Auto Save</span>
            </div>
          </div>

          <div class="setting-group">
            <div class="setting-title">Telemetry</div>
            <div class="setting-desc">Enable crash reports and usage data collection to help improve Vrutti.</div>
            <div class="toggle-container">
              <input type="checkbox" class="checkbox" .checked=${this.config['telemetry.enableTelemetry']} 
                     @change=${(e: any) => this.handleSettingChange('telemetry.enableTelemetry', e.target.checked)} />
              <span>Send Telemetry Data</span>
            </div>
          </div>
        `;
      case 'Keybindings':
      default:
        return html`
          <div style="color: var(--vrutti-surface-border); margin-top: 40px; text-align: center;">
            No settings found in this category.
          </div>
        `;
    }
  }
}
