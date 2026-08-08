import { LitElement, html, css } from 'lit';
import { customElement, property, query, state } from 'lit/decorators.js';
import { EditorState, StateField, StateEffect, RangeSet } from '@codemirror/state';
import { EditorView, keymap, lineNumbers, highlightActiveLineGutter, highlightSpecialChars, drawSelection, dropCursor, rectangularSelection, crosshairCursor, highlightActiveLine, gutter, GutterMarker } from '@codemirror/view';
import { defaultKeymap, history, historyKeymap } from '@codemirror/commands';
import { searchKeymap, highlightSelectionMatches } from '@codemirror/search';
import { autocompletion, completionKeymap, closeBrackets, closeBracketsKeymap } from '@codemirror/autocomplete';
import { lintKeymap } from '@codemirror/lint';
import { bracketMatching, foldGutter, foldKeymap, indentOnInput, syntaxHighlighting, defaultHighlightStyle } from '@codemirror/language';
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
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 14px;
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
        }
    };

    constructor() {
        super();
        window.addEventListener('setting-changed', this._settingsHandler as EventListener);
        window.addEventListener('editor-action', this._editorActionHandler as EventListener);
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
                    } else {
                        this.initEditor();
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
            oneDark,
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
