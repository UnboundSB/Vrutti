import { LitElement, html, css } from 'lit';
import { customElement, property, query } from 'lit/decorators.js';
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
            background-color: var(--vrutti-bg-dark, #1e1e1e);
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

    private _editorView?: EditorView;
    private _fileContent = '';
    private _saveTimeout?: number;

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
                this._saveTimeout = window.setTimeout(() => {
                    this.saveFile();
                }, 800); // Auto-save after 800ms of inactivity
            }
        });

        const state = EditorState.create({
            doc: this._fileContent,
            extensions: [
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
            ]
        });

        this._editorView = new EditorView({
            state,
            parent: this.editorContainer
        });
    }

    disconnectedCallback() {
        super.disconnectedCallback();
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
