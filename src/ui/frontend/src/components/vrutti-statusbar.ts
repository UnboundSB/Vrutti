import { LitElement, css, html } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { unsafeSVG } from 'lit/directives/unsafe-svg.js';
import { icon_git_branch, icon_error, icon_warning } from './codicons';

@customElement('vrutti-statusbar')
export class VruttiStatusBar extends LitElement {

  @state()
  private branch = 'main';

  @state()
  private errors = 0;

  @state()
  private warnings = 0;

  @state()
  private line = 1;

  @state()
  private col = 1;

  @state()
  private indent = 'Spaces: 4';

  @state()
  private encoding = 'UTF-8';

  @state()
  private eol = 'CRLF';

  @state()
  private language = 'TypeScript';

  static styles = css`
    :host {
      display: flex;
      height: 22px;
      width: 100%;
      background-color: var(--vrutti-bg, #0f111a);
      border-top: 1px solid var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text, #636b95);
      font-family: var(--vrutti-font, 'Inter', sans-serif);
      font-size: 11px;
      user-select: none;
      align-items: center;
      justify-content: space-between;
      box-sizing: border-box;
      padding: 0 8px;
    }

    .section {
      display: flex;
      align-items: center;
      height: 100%;
    }

    .item {
      display: flex;
      align-items: center;
      height: 100%;
      padding: 0 6px;
      cursor: pointer;
      transition: background-color 0.1s ease, color 0.1s ease;
    }

    .item:hover {
      background-color: var(--vrutti-surface-border, #23273b);
      color: var(--vrutti-text-bright, #a6accd);
    }

    .item svg {
      width: 14px;
      height: 14px;
      margin-right: 4px;
    }

    .errors-warnings {
      display: flex;
      align-items: center;
    }

    .errors-warnings span {
      margin-right: 6px;
    }
  `;

  render() {
    return html`
      <div class="section left">
        <div class="item" title="Git Branch">
          ${unsafeSVG(icon_git_branch)}
          <span>${this.branch}</span>
        </div>
        <div class="item errors-warnings" title="0 Errors, 0 Warnings">
          ${unsafeSVG(icon_error)} <span>${this.errors}</span>
          ${unsafeSVG(icon_warning)} <span>${this.warnings}</span>
        </div>
      </div>
      <div class="section right">
        <div class="item" title="Go to Line/Column">
          Ln ${this.line}, Col ${this.col}
        </div>
        <div class="item" title="Select Indentation">
          ${this.indent}
        </div>
        <div class="item" title="Select Encoding">
          ${this.encoding}
        </div>
        <div class="item" title="Select End of Line Sequence">
          ${this.eol}
        </div>
        <div class="item" title="Select Language Mode">
          ${this.language}
        </div>
      </div>
    `;
  }
}
