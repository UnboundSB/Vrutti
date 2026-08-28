import { LitElement, html, css } from 'lit';
import { customElement, property, query, state } from 'lit/decorators.js';
import { MergeView } from '@codemirror/merge';
import { EditorState } from '@codemirror/state';
import { EditorView, lineNumbers, drawSelection, dropCursor, rectangularSelection, highlightActiveLine } from '@codemirror/view';
import { highlightSelectionMatches } from '@codemirror/search';
import { closeBrackets } from '@codemirror/autocomplete';
import { bracketMatching, syntaxHighlighting, defaultHighlightStyle, HighlightStyle } from '@codemirror/language';
import { tags as t } from '@lezer/highlight';
import { oneDark } from '@codemirror/theme-one-dark';
import { javascript } from '@codemirror/lang-javascript';
import { cpp } from '@codemirror/lang-cpp';
import { json } from '@codemirror/lang-json';
import { python } from '@codemirror/lang-python';

@customElement('vrutti-diff-editor')
export class VruttiDiffEditor extends LitElement {
    static styles = css`
        :host {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background-color: var(--vrutti-bg, #1e1e1e);
            overflow: hidden;
            min-width: 0;
            min-height: 0;
        }

        #diff-container {
            flex: 1 1 0%;
            overflow: hidden;
            position: relative;
            min-width: 0;
            min-height: 0;
            display: flex;
        }

        .cm-mergeView {
            height: 100%;
            width: 100%;
            overflow: hidden;
        }

        .cm-mergeViewEditors {
            height: 100%;
            width: 100%;
        }

        .cm-editor {
            height: 100%;
        }

        .cm-scroller {
            font-family: var(--vrutti-editor-font, 'Consolas', 'Courier New', monospace);
            font-size: var(--vrutti-editor-font-size, 14px);
        }

        ::-webkit-scrollbar {
            width: 10px;
            height: 10px;
            background: transparent;
        }
        ::-webkit-scrollbar-thumb {
            background: transparent;
            border-radius: 5px;
        }
        :hover::-webkit-scrollbar-thumb {
            background-color: rgba(122, 162, 247, 0.4);
        }
    `;

    @property({ type: String }) originalPath = '';
    @property({ type: String }) modifiedPath = '';

    @query('#diff-container') diffContainer!: HTMLElement;

    @state() private _originalContent: string = '';
    @state() private _modifiedContent: string = '';
    
    private _mergeView?: MergeView;
    private _isDarkTheme: boolean = true;
    
    private _themeHandler = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (this._isDarkTheme !== detail.isDark) {
            this._isDarkTheme = detail.isDark;
            this.initEditor();
        }
    };

    constructor() {
        super();
        try {
            const applied = localStorage.getItem('vrutti-applied-theme');
            if (applied) {
                const t = JSON.parse(applied);
                this._isDarkTheme = t.uiTheme === 'vs-dark';
            }
        } catch(e) {}
    }

    connectedCallback() {
        super.connectedCallback();
        window.addEventListener('theme-loaded', this._themeHandler as EventListener);
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('theme-loaded', this._themeHandler as EventListener);
        if (this._mergeView) {
            this._mergeView.destroy();
        }
    }

    async firstUpdated() {
        if (this.originalPath && this.modifiedPath) {
            await this.loadFiles();
            this.initEditor();
        }
    }

    updated(changedProperties: Map<string, any>) {
        if (changedProperties.has('originalPath') || changedProperties.has('modifiedPath')) {
            this.loadFiles().then(() => {
                this.initEditor();
            });
        }
    }

    private getLanguageExtension(filePath: string) {
        const ext = filePath.split('.').pop()?.toLowerCase();
        switch (ext) {
            case 'js':
            case 'ts':
                return javascript({ typescript: ext === 'ts' });
            case 'cpp':
            case 'h':
            case 'c':
            case 'hpp':
                return cpp();
            case 'json':
                return json();
            case 'py':
                return python();
            default:
                return [];
        }
    }

    private buildHighlightStyle(tokenColors: any[]) {
        const specs: any[] = [];
        let keywordColor, stringColor, functionColor, numberColor, commentColor, variableColor, typeColor, operatorColor;
        
        for (const rule of tokenColors) {
            const scope = rule.scope;
            const settings = rule.settings;
            if (!settings || !settings.foreground) continue;
            
            const scopes = Array.isArray(scope) ? scope : (scope ? scope.split(',') : []);
            for (let s of scopes) {
                s = s.trim();
                if (s.startsWith('keyword') || s.startsWith('storage')) keywordColor = keywordColor || settings.foreground;
                if (s.startsWith('string')) stringColor = stringColor || settings.foreground;
                if (s.startsWith('entity.name.function') || s.startsWith('support.function')) functionColor = functionColor || settings.foreground;
                if (s.startsWith('constant.numeric')) numberColor = numberColor || settings.foreground;
                if (s.startsWith('comment')) commentColor = commentColor || settings.foreground;
                if (s.startsWith('variable') || s.startsWith('entity.name.variable')) variableColor = variableColor || settings.foreground;
                if (s.startsWith('entity.name.type') || s.startsWith('support.type') || s.startsWith('entity.name.class') || s.startsWith('support.class')) typeColor = typeColor || settings.foreground;
                if (s.startsWith('keyword.operator')) operatorColor = operatorColor || settings.foreground;
            }
        }
        
        if (keywordColor) specs.push({ tag: [t.keyword, t.modifier, t.controlKeyword, t.moduleKeyword], color: keywordColor });
        if (stringColor) specs.push({ tag: [t.string, t.special(t.string)], color: stringColor });
        if (functionColor) specs.push({ tag: [t.function(t.variableName), t.function(t.propertyName)], color: functionColor });
        if (numberColor) specs.push({ tag: [t.number, t.bool, t.integer, t.float], color: numberColor });
        if (commentColor) specs.push({ tag: [t.lineComment, t.blockComment, t.comment], color: commentColor, fontStyle: 'italic' });
        if (variableColor) specs.push({ tag: [t.variableName, t.propertyName, t.name], color: variableColor });
        if (typeColor) specs.push({ tag: [t.typeName, t.className, t.namespace], color: typeColor });
        if (operatorColor) specs.push({ tag: [t.operator, t.arithmeticOperator, t.logicOperator], color: operatorColor });
        
        return HighlightStyle.define(specs);
    }

    private getThemeExtension() {
        try {
            const applied = localStorage.getItem('vrutti-applied-theme');
            if (applied) {
                const tTheme = JSON.parse(applied);
                if (tTheme.tokenColors) {
                    const style = this.buildHighlightStyle(tTheme.tokenColors);
                    const bg = tTheme.colors?.['--vrutti-bg'] || (this._isDarkTheme ? '#1e1e1e' : '#ffffff');
                    const fg = tTheme.colors?.['--vrutti-text-bright'] || tTheme.colors?.['--vrutti-text'] || (this._isDarkTheme ? '#d4d4d4' : '#333333');
                    const selection = tTheme.colors?.['editor.selectionBackground'] || (this._isDarkTheme ? '#264f78' : '#add6ff');
                    
                    const baseTheme = EditorView.theme({
                        "&": { color: fg, backgroundColor: bg },
                        ".cm-content": { caretColor: fg },
                        ".cm-cursor, .cm-dropCursor": { borderLeftColor: fg },
                        "&.cm-focused .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection": { backgroundColor: selection },
                        ".cm-activeLine": { backgroundColor: "transparent" },
                        ".cm-gutters": { backgroundColor: bg, color: fg, borderRight: "none" }
                    }, { dark: this._isDarkTheme });
                    
                    return [baseTheme, syntaxHighlighting(style)];
                }
            }
        } catch(e) {}
        
        return this._isDarkTheme ? oneDark : [];
    }

    private async loadFiles() {
        try {
            let origPath = this.originalPath;
            if (origPath.startsWith('file:///')) origPath = origPath.substring(8);
            else if (origPath.startsWith('file://')) origPath = origPath.substring(7);

            let modPath = this.modifiedPath;
            if (modPath.startsWith('file:///')) modPath = modPath.substring(8);
            else if (modPath.startsWith('file://')) modPath = modPath.substring(7);

            this._originalContent = await (window as any).vruttiReadFile(origPath);
            this._modifiedContent = await (window as any).vruttiReadFile(modPath);
        } catch (e) {
            console.error("Failed to load files for diff:", e);
        }
    }

    private initEditor() {
        if (this._mergeView) {
            this._mergeView.destroy();
        }

        const commonExtensions = [
            lineNumbers(),
            drawSelection(),
            dropCursor(),
            EditorState.allowMultipleSelections.of(true),
            syntaxHighlighting(defaultHighlightStyle, { fallback: true }),
            bracketMatching(),
            closeBrackets(),
            rectangularSelection(),
            highlightActiveLine(),
            highlightSelectionMatches(),
            this.getThemeExtension()
        ];

        this._mergeView = new MergeView({
            a: {
                doc: this._originalContent,
                extensions: [
                    ...commonExtensions,
                    EditorView.editable.of(false),
                    this.getLanguageExtension(this.originalPath)
                ]
            },
            b: {
                doc: this._modifiedContent,
                extensions: [
                    ...commonExtensions,
                    EditorView.editable.of(false),
                    this.getLanguageExtension(this.modifiedPath)
                ]
            },
            parent: this.diffContainer
        });
    }

    render() {
        return html`
            <div id="diff-container"></div>
        `;
    }
}
