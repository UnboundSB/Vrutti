import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { registry, ConfigurationSchema, ConfigurationProperty } from '../core/Registry';
import { globalHoverStyle } from '../shared-styles';

@customElement('vrutti-settings')
export class VruttiSettings extends LitElement {
  @state() private activeCategory = 'General';
  @state() private configValues: Record<string, any> = {};
  @state() private configurations: ConfigurationSchema[] = [];
  @state() private availableThemes: { id: string, label: string }[] = [];
  @state() private appliedTheme: string = '';
  @state() private selectedTheme: string = '';
  @state() private showDirtyModal = false;

  private saveTimeout: number | null = null;

  connectedCallback() {
    super.connectedCallback();
    console.log('[Settings] Loaded lazy component');
    
    this.updateFromRegistry();
    registry.addEventListener('change', this.handleRegistryChange);

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
    registry.removeEventListener('change', this.handleRegistryChange);
    window.removeEventListener('vrutti-ipc', this.handleIpc as EventListener);
    if (this.saveTimeout) {
      window.clearTimeout(this.saveTimeout);
      this.saveTimeout = null;
    }
  }

  private handleRegistryChange = (e: Event) => {
    const type = (e as CustomEvent).detail;
    if (type === 'configurations') {
        this.updateFromRegistry();
    }
  };

  private updateFromRegistry() {
      this.configurations = registry.getConfigurations();
      if (this.configurations.length > 0 && !this.configurations.find(c => c.title === this.activeCategory) && this.activeCategory !== 'Theme' && this.activeCategory !== 'Keybindings') {
          this.activeCategory = this.configurations[0].title;
      }
      
      // Initialize defaults
      const newConfig: Record<string, any> = { ...this.configValues };
      for (const schema of this.configurations) {
          for (const [key, prop] of Object.entries(schema.properties)) {
              if (newConfig[key] === undefined) {
                  newConfig[key] = prop.default;
              }
          }
      }
      this.configValues = newConfig;
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
    this.handleSettingChange('workbench.colorTheme', this.selectedTheme);
  }

  private applyAndExit() {
    this.apply();
    this.dispatchEvent(new CustomEvent('close-settings', { bubbles: true, composed: true }));
  }

  private handleSettingChange(key: string, value: any) {
    this.configValues = { ...this.configValues, [key]: value };
    
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
        detail: { key, value: this.configValues[key] },
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
            ${this.configurations.map(c => html`
            <div class="category-item ${this.activeCategory === c.title ? 'active' : ''}"
                 @click=${() => this.activeCategory = c.title}>
              ${c.title}
            </div>
          `)}
            <div class="category-item ${this.activeCategory === 'Theme' ? 'active' : ''}"
                 @click=${() => this.activeCategory = 'Theme'}>
              Theme
            </div>
            <div class="category-item ${this.activeCategory === 'Keybindings' ? 'active' : ''}"
                 @click=${() => this.activeCategory = 'Keybindings'}>
              Keybindings
            </div>
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

  private renderProperty(key: string, prop: ConfigurationProperty) {
    if (prop.type === 'boolean') {
        return html`
            <div class="setting-group">
                <div class="setting-title">${key}</div>
                <div class="setting-desc">${prop.description}</div>
                <div class="toggle-container">
                    <input type="checkbox" class="checkbox" .checked=${this.configValues[key]} 
                           @change=${(e: any) => this.handleSettingChange(key, e.target.checked)} />
                    <span>Enable</span>
                </div>
            </div>
        `;
    } else if (prop.type === 'number') {
        return html`
            <div class="setting-group">
                <div class="setting-title">${key}</div>
                <div class="setting-desc">${prop.description}</div>
                <input type="number" class="input-field" .value=${this.configValues[key]?.toString()} 
                       @input=${(e: any) => this.handleSettingChange(key, parseInt(e.target.value))} />
            </div>
        `;
    } else if (prop.type === 'enum' && prop.enum) {
        return html`
            <div class="setting-group">
                <div class="setting-title">${key}</div>
                <div class="setting-desc">${prop.description}</div>
                <select class="input-field" @change=${(e: any) => this.handleSettingChange(key, e.target.value)}>
                    ${prop.enum.map(opt => html`<option value="${opt}" ?selected=${opt === this.configValues[key]}>${opt}</option>`)}
                </select>
            </div>
        `;
    } else {
        // string
        return html`
            <div class="setting-group">
                <div class="setting-title">${key}</div>
                <div class="setting-desc">${prop.description}</div>
                <input type="text" class="input-field" .value=${this.configValues[key] || ''} 
                       @input=${(e: any) => this.handleSettingChange(key, e.target.value)} />
            </div>
        `;
    }
  }

  private renderCategoryContent() {
    if (this.activeCategory === 'Theme') {
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
    } else if (this.activeCategory === 'Keybindings') {
        return html`
          <div style="color: var(--vrutti-surface-border); margin-top: 40px; text-align: center;">
            No settings found in this category.
          </div>
        `;
    }

    const schema = this.configurations.find(c => c.title === this.activeCategory);
    if (!schema) {
        return html`
          <div style="color: var(--vrutti-surface-border); margin-top: 40px; text-align: center;">
            No settings found in this category.
          </div>
        `;
    }

    return html`
        ${Object.entries(schema.properties).map(([key, prop]) => this.renderProperty(key, prop))}
    `;
  }
}
