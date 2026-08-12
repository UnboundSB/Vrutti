import { LitElement, html, css } from 'lit';
import { customElement, property, query, state } from 'lit/decorators.js';
import { EditorState, StateField, StateEffect, RangeSet, RangeSetBuilder } from '@codemirror/state';
import { EditorView, keymap, lineNumbers, highlightActiveLineGutter, highlightSpecialChars, drawSelection, dropCursor, rectangularSelection, crosshairCursor, highlightActiveLine, gutter, GutterMarker, Decoration, DecorationSet } from '@codemirror/view';
import { defaultKeymap, history, historyKeymap } from '@codemirror/commands';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { autocompletion, completionKeymap, closeBrackets, closeBracketsKeymap } from '@codemirror/autocomplete';
import { lintKeymap } from '@codemirror/lint';
import { bracketMatching, foldGutter, foldKeymap, indentOnInput, syntaxHighlighting, defaultHighlightStyle, HighlightStyle } from '@codemirror/language';
import { tags as t } from '@lezer/highlight';
import { oneDark } from '@codemirror/theme-one-dark';
import { javascript } from '@codemirror/lang-javascript';
import { cpp } from '@codemirror/lang-cpp';
import { json } from '@codemirror/lang-json';
import { python } from '@codemirror/lang-python';

const breakpointEffect = StateEffect.define<{pos: number, on: boolean}>({
    map: (val, mapping) => ({pos: mapping.mapPos(val.pos), on: val.on})
});

const breakpointMarker = new class extends GutterMarker {
    toDOM() {
        let span = document.createElement("span");
        span.className = "cm-breakpoint";
        return span;
    }
};

const breakpointState = StateField.define<RangeSet<GutterMarker>>({
    create() { return RangeSet.empty; },
    update(set, transaction) {
        set = set.map(transaction.changes);
        for (let e of transaction.effects) {
            if (e.is(breakpointEffect)) {
                if (e.value.on)
                    set = set.update({add: [breakpointMarker.range(e.value.pos)]});
                else
                    set = set.update({filter: from => from !== e.value.pos});
            }
        }
        return set;
    }
});

function toggleBreakpoint(view: EditorView, pos: number) {
    let breakpoints = view.state.field(breakpointState);
    let hasBreakpoint = false;
    breakpoints.between(pos, pos, () => {hasBreakpoint = true});
    view.dispatch({
        effects: breakpointEffect.of({pos, on: !hasBreakpoint})
    });
}

const breakpointGutter = [
    breakpointState,
    gutter({
        class: "cm-breakpoint-gutter",
        markers: v => v.state.field(breakpointState),
        initialSpacer: () => breakpointMarker,
        domEventHandlers: {
            mousedown(view, line) {
                toggleBreakpoint(view, line.from);
                return true;
            }
        }
    })
];

export const setTokensEffect = StateEffect.define<any[]>();

export const textmateTokensField = StateField.define<DecorationSet>({
    create() { return Decoration.none; },
    update(decorations, tr) {
        let newDecorations = decorations.map(tr.changes);
        for (let e of tr.effects) {
            if (e.is(setTokensEffect)) {
                const tokensArray = e.value;
                const builder = new RangeSetBuilder<Decoration>();
                const doc = tr.state.doc;
                const tokenColors = (window as any).vruttiActiveThemeTokenColors || [];
                
                for (let i = 0; i < tokensArray.length; i++) {
                    if (i >= doc.lines) break;
                    const line = doc.line(i + 1);
                    const lineTokens = tokensArray[i];
                    for (const t of lineTokens) {
                        if (t.startIndex === t.endIndex) continue;
                        
                        let color = null;
                        for (let j = t.scopes.length - 1; j >= 0; j--) {
                            const scope = t.scopes[j];
                            for (const rule of tokenColors) {
                                if (!rule.scope) continue;
                                const ruleScopes = Array.isArray(rule.scope) ? rule.scope : rule.scope.split(',').map((s: string) => s.trim());
                                for (const rs of ruleScopes) {
                                    if (scope === rs || scope.startsWith(rs + '.')) {
                                        if (rule.settings && rule.settings.foreground) {
                                            color = rule.settings.foreground;
                                            break;
                                        }
                                    }
                                }
                                if (color) break;
                            }
                            if (color) break;
                        }
                        
                        if (color) {
                            builder.add(line.from + t.startIndex, line.from + t.endIndex, Decoration.mark({
                                attributes: { style: `color: ${color}` }
                            }));
                        }
                    }
                }
                newDecorations = builder.finish();
            }
        }
        return newDecorations;
    },
    provide: f => EditorView.decorations.from(f)
});

@customElement('vrutti-editor')
export class VruttiEditor extends LitElement {
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

        #editor-container {
            flex: 1 1 0%;
            overflow: hidden;
            position: relative;
            min-width: 0;
            min-height: 0;
        }

        .cm-editor {
            height: 100%;
        }

        .cm-scroller {
            font-family: var(--vrutti-editor-font, 'Consolas', 'Courier New', monospace);
            font-size: var(--vrutti-editor-font-size, 14px);
        }
        
        .cm-breakpoint-gutter {
            width: 14px;
            cursor: pointer;
        }
        
        .cm-breakpoint {
            display: inline-block;
            width: 10px;
            height: 10px;
            background-color: #f7768e;
            border-radius: 50%;
            margin-top: 5px;
            margin-left: 6px;
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

    @property({ type: String }) filePath = '';
    @query('#editor-container') editorContainer!: HTMLElement;

    @state() private _fileContent: string = '';
    private _editorView?: EditorView;
    private _saveTimeout?: number;
    private _wordWrap: boolean = false;
    private _autoSave: boolean = true;

    private _settingsHandler = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (detail.key === 'editor.wordWrap') {
            this._wordWrap = detail.value;
            if (this._editorView) {
                // Reconfigure word wrap dynamically
                this.initEditor();
            }
        } else if (detail.key === 'files.autoSave') {
            this._autoSave = detail.value;
        } else if (detail.key === 'editor.fontSize') {
            this.style.setProperty('--vrutti-editor-font-size', `${detail.value}px`);
        } else if (detail.key === 'editor.fontFamily') {
            this.style.setProperty('--vrutti-editor-font', detail.value);
        }
    };

    private _isDarkTheme: boolean = true;
    
    private _themeHandler = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (this._isDarkTheme !== detail.isDark) {
            this._isDarkTheme = detail.isDark;
            if (this._editorView) {
                this.initEditor();
                this.requestTokens();
            }
        }
    };
    
    private _ipcHandler = (e: Event) => {
        const msg = (e as CustomEvent).detail;
        if (msg.method === 'editor/tokens' && msg.params && msg.params.fileId === this.filePath) {
            if (this._editorView) {
                this._editorView.dispatch({
                    effects: setTokensEffect.of(msg.params.tokens)
                });
            }
        }
    };

    private requestTokens() {
        if (!this.filePath || !this._fileContent) return;
        const ext = this.filePath.split('.').pop()?.toLowerCase();
        let langId = 'plaintext';
        if (ext === 'js' || ext === 'ts') langId = 'javascript'; // roughly map
        else if (ext === 'cpp' || ext === 'h') langId = 'cpp';
        else if (ext === 'py') langId = 'python';
        else if (ext === 'json') langId = 'json';
        else langId = ext || 'plaintext';
        
        window.dispatchEvent(new CustomEvent('vrutti-ipc', {
            detail: {
                method: 'editor/tokenize',
                languageId: langId,
                fileId: this.filePath,
                text: this._fileContent
            }
        }));
    }

    constructor() {
        super();
        window.addEventListener('setting-changed', this._settingsHandler as EventListener);
        window.addEventListener('editor-action', this._editorActionHandler as EventListener);
        window.addEventListener('theme-loaded', this._themeHandler as EventListener);
        window.addEventListener('vrutti-ipc', this._ipcHandler as EventListener);
        try {
            const applied = localStorage.getItem('vrutti-applied-theme');
            if (applied) {
                const t = JSON.parse(applied);
                this._isDarkTheme = t.uiTheme === 'vs-dark';
            }
        } catch(e) {}
    }

    private _editorActionHandler = (e: Event) => {
        const detail = (e as CustomEvent).detail;
        if (!this._editorView) return;
        const view = this._editorView;
        
        switch (detail.action) {
            case 'Undo':
                import('@codemirror/commands').then(m => m.undo(view));
                break;
            case 'Redo':
                import('@codemirror/commands').then(m => m.redo(view));
                break;
            case 'Cut':
                document.execCommand('cut');
                break;
            case 'Copy':
                document.execCommand('copy');
                break;
            case 'Paste':
                navigator.clipboard.readText().then(text => {
                   const selection = view.state.selection.main;
                   view.dispatch({
                       changes: {from: selection.from, to: selection.to, insert: text},
                       selection: {anchor: selection.from + text.length}
                   });
                }).catch(() => document.execCommand('paste'));
                break;
            case 'Find':
            case 'Replace':
                import('@codemirror/search').then(m => m.openSearchPanel(view));
                break;
            case 'Select All':
                import('@codemirror/commands').then(m => m.selectAll(view));
                break;
            case 'Expand Selection':
                import('@codemirror/commands').then(m => m.selectLine(view));
                break;
            case 'Add Cursor Above':
                // best effort mapping if available
                import('@codemirror/commands').then(m => { if(m.cursorLineUp) m.cursorLineUp(view); });
                break;
            case 'Add Cursor Below':
                import('@codemirror/commands').then(m => { if(m.cursorLineDown) m.cursorLineDown(view); });
                break;
            case 'Add Cursors to Line Ends':
                // Not standard CM6 but we'll try if a similar command exists
                break;
            case 'Add Next Occurrence':
                import('@codemirror/search').then(m => { if(m.selectNextOccurrence) m.selectNextOccurrence(view); });
                break;
            case 'Add Previous Occurrence':
                // Not standard but might exist in search
                break;
            case 'Select All Occurrences':
                import('@codemirror/search').then(m => { if(m.selectMatches) m.selectMatches(view); });
                break;
            case 'Toggle Line Comment':
                import('@codemirror/commands').then(m => { if(m.toggleLineComment) m.toggleLineComment(view); });
                break;
            case 'Toggle Block Comment':
                import('@codemirror/commands').then(m => { if(m.toggleBlockComment) m.toggleBlockComment(view); });
                break;
            case 'Shrink Selection':
                break;
            case 'Copy Line Up':
                import('@codemirror/commands').then(m => { if(m.copyLineUp) m.copyLineUp(view); });
                break;
            case 'Copy Line Down':
                import('@codemirror/commands').then(m => { if(m.copyLineDown) m.copyLineDown(view); });
                break;
            case 'Move Line Up':
                import('@codemirror/commands').then(m => { if(m.moveLineUp) m.moveLineUp(view); });
                break;
            case 'Move Line Down':
                import('@codemirror/commands').then(m => { if(m.moveLineDown) m.moveLineDown(view); });
                break;
            case 'Duplicate Selection':
                // Generic duplicate line/selection fallback if exact command missing
                import('@codemirror/commands').then(m => { if(m.copyLineDown) m.copyLineDown(view); });
                break;
            case 'Go to Line/Column':
                import('@codemirror/search').then(m => { if(m.gotoLine) m.gotoLine(view); });
                break;
            case 'Go to Definition':
            case 'Go to Declaration':
            case 'Go to Type Definition':
            case 'Go to Implementations':
            case 'Go to References':
                window.alert(`Language server required for ${detail.action}`);
                break;
            case 'Next Problem':
                import('@codemirror/lint').then(m => { if(m.nextDiagnostic) m.nextDiagnostic(view); });
                break;
            case 'Previous Problem':
                import('@codemirror/lint').then(m => { if(m.previousDiagnostic) m.previousDiagnostic(view); });
                break;
            case 'Save':
                this.saveFile();
                break;
        }
    };

    async firstUpdated() {
        if (this.filePath) {
            await this.loadFile();
            this.initEditor();
        }
    }

    updated(changedProperties: Map<string, any>) {
        if (changedProperties.has('filePath')) {
            if (this.filePath !== changedProperties.get('filePath')) {
                this.loadFile().then(() => {
                    if (this._editorView) {
                        this._editorView.dispatch({
                            changes: { from: 0, to: this._editorView.state.doc.length, insert: this._fileContent }
                        });
                        this.requestTokens();
                    } else {
                        this.initEditor();
                        this.requestTokens();
                    }
                });
            }
        }
    }



    private getLanguageExtension() {
        const ext = this.filePath.split('.').pop()?.toLowerCase();
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
                return []; // No specific language, default behavior
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
                        (window as any).vruttiActiveThemeTokenColors = tTheme.tokenColors;
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
                        
                        return [baseTheme, syntaxHighlighting(style), textmateTokensField];
                    }
            }
        } catch(e) {
            console.error('Error generating theme extension', e);
        }
        
        return this._isDarkTheme ? oneDark : [];
    }

    private async loadFile() {
        try {
            let actualPath = this.filePath;
            if (actualPath.startsWith('file:///')) actualPath = actualPath.substring(8);
            else if (actualPath.startsWith('file://')) actualPath = actualPath.substring(7);

            // Read file properly using the C++ backend
            const rawContent = await (window as any).vruttiReadFile(actualPath);
            this._fileContent = rawContent;
        } catch (e) {
            console.error("Failed to load file:", e);
            this._fileContent = "// Failed to load file.";
        }
    }

    private async saveFile() {
        if (!this._editorView) return;
        const currentContent = this._editorView.state.doc.toString();
        try {
            let actualPath = this.filePath;
            if (actualPath.startsWith('file:///')) actualPath = actualPath.substring(8);
            else if (actualPath.startsWith('file://')) actualPath = actualPath.substring(7);

            const result = await (window as any).vruttiWriteFile(actualPath, currentContent);
            const parsed = JSON.parse(result);
            if (parsed.success) {
                console.log("File saved successfully.");
            } else {
                console.error("Failed to save file.");
            }
        } catch (e) {
            console.error("Error saving file:", e);
        }
    }

    private initEditor() {
        if (this._editorView) {
            this._editorView.destroy();
        }

        const saveKeymap = keymap.of([
            {
                key: "Mod-s",
                run: () => {
                    this.saveFile();
                    return true;
                },
                preventDefault: true
            },
            {
                key: "Enter",
                run: () => {
                    this.saveFile();
                    return false; // let the default enter behavior continue (insert newline)
                }
            }
        ]);

        const updateListener = EditorView.updateListener.of((update) => {
            if (update.selectionSet || update.docChanged) {
                const head = update.state.selection.main.head;
                const line = update.state.doc.lineAt(head);
                window.dispatchEvent(new CustomEvent('editor-cursor-changed', {
                    detail: { line: line.number, col: head - line.from + 1 }
                }));
            }
            if (update.docChanged) {
                this._fileContent = update.state.doc.toString();
                if ((this as any)._tokenTimeout) clearTimeout((this as any)._tokenTimeout);
                (this as any)._tokenTimeout = setTimeout(() => {
                    this.requestTokens();
                }, 200); // Debounce tokenization

                if (this._saveTimeout) {
                    clearTimeout(this._saveTimeout);
                }
                if (this._autoSave) {
                    this._saveTimeout = window.setTimeout(() => {
                        this.saveFile();
                    }, 800); // Auto-save after 800ms of inactivity
                }
            }
        });

        const extensions = [
            breakpointGutter,
            lineNumbers({
                domEventHandlers: {
                    mousedown(view, line) {
                        toggleBreakpoint(view, line.from);
                        return true;
                    }
                }
            }),
            highlightActiveLineGutter(),
            highlightSpecialChars(),
            history(),
            foldGutter(),
            drawSelection(),
            dropCursor(),
            EditorState.allowMultipleSelections.of(true),
            indentOnInput(),
            syntaxHighlighting(defaultHighlightStyle, { fallback: true }),
            bracketMatching(),
            closeBrackets(),
            autocompletion(),
            rectangularSelection(),
            crosshairCursor(),
            highlightActiveLine(),
            highlightSelectionMatches(),
            keymap.of([
                ...closeBracketsKeymap,
                ...defaultKeymap,
                ...searchKeymap,
                ...historyKeymap,
                ...foldKeymap,
                ...completionKeymap,
                ...lintKeymap
            ]),
            this.getThemeExtension(),
            saveKeymap,
            updateListener,
            this.getLanguageExtension()
        ];

        if (this._wordWrap) {
            extensions.push(EditorView.lineWrapping);
        }

        const state = EditorState.create({
            doc: this._fileContent,
            extensions: extensions
        });

        this._editorView = new EditorView({
            state,
            parent: this.editorContainer
        });
    }

    disconnectedCallback() {
        super.disconnectedCallback();
        window.removeEventListener('setting-changed', this._settingsHandler as EventListener);
        window.removeEventListener('editor-action', this._editorActionHandler as EventListener);
        window.removeEventListener('theme-loaded', this._themeHandler as EventListener);
        window.removeEventListener('vrutti-ipc', this._ipcHandler as EventListener);
        if (this._saveTimeout) {
            clearTimeout(this._saveTimeout);
            this._saveTimeout = undefined;
        }
        if (this._editorView) {
            this._editorView.destroy();
        }
    }



    render() {
        return html`
            <div id="editor-container"></div>
        `;
    }
}
