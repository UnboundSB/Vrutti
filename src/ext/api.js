class VruttiAPI {
    constructor(ipcClient) {
        this.ipcClient = ipcClient;
        
        // This object acts as the polyfill for the extension API.
        // It simply forwards requests to the native C++ engine via IPC.
        
        this.window = {
            showInformationMessage: async (message) => {
                return this.ipcClient.sendRequest('window/showInformationMessage', { message });
            },
            showErrorMessage: async (message) => {
                return this.ipcClient.sendRequest('window/showErrorMessage', { message });
            },
            showWarningMessage: async (message) => {
                return this.ipcClient.sendRequest('window/showInformationMessage', { message: "[Warning] " + message });
            },
            setStatusBarMessage: (text, hideAfterTimeout) => {
                return { dispose: () => {} };
            },
            createOutputChannel: (name) => {
                return {
                    name,
                    append: (value) => this.ipcClient.sendNotification('run/output', { text: value }),
                    appendLine: (value) => this.ipcClient.sendNotification('run/output', { text: value + '\n' }),
                    clear: () => {},
                    show: () => {},
                    hide: () => {},
                    dispose: () => {}
                };
            },
            activeTextEditor: undefined,
            visibleTextEditors: [],
            onDidChangeActiveTextEditor: () => ({ dispose: () => {} }),
            onDidChangeTextEditorSelection: () => ({ dispose: () => {} }),
            onDidChangeVisibleTextEditors: () => ({ dispose: () => {} }),
            createTerminal: (nameOrOptions) => {
                return {
                    name: typeof nameOrOptions === 'string' ? nameOrOptions : (nameOrOptions ? nameOrOptions.name : 'Terminal'),
                    processId: Promise.resolve(0),
                    sendText: (text, addNewLine = true) => {
                        this.ipcClient.sendNotification('terminal/runCommand', { command: text });
                    },
                    show: () => {},
                    hide: () => {},
                    dispose: () => {}
                };
            }
        };

        this.workspace = {
            getRootPath: async () => {
                return this.ipcClient.sendRequest('workspace/getRootPath');
            },
            openTextDocument: async (uri) => {
                return this.ipcClient.sendRequest('workspace/openTextDocument', { uri });
            },
            getConfiguration: (section, scope) => {
                return {
                    get: (key, defaultValue) => defaultValue,
                    has: () => false,
                    inspect: () => undefined,
                    update: async () => {}
                };
            },
            onDidChangeConfiguration: () => ({ dispose: () => {} }),
            onDidSaveTextDocument: () => ({ dispose: () => {} }),
            onDidOpenTextDocument: () => ({ dispose: () => {} }),
            onDidCloseTextDocument: () => ({ dispose: () => {} })
        };

        this._commandRegistry = new Map();
        
        this.commands = {
            registerCommand: (commandId, callback) => {
                this._commandRegistry.set(commandId, callback);
                return { dispose: () => this._commandRegistry.delete(commandId) };
            },
            executeCommand: async (commandId, ...args) => {
                const callback = this._commandRegistry.get(commandId);
                if (callback) {
                    return await callback(...args);
                }
                // Fallback for missing commands, might be native
                return undefined;
            },
            getCommands: async () => Array.from(this._commandRegistry.keys())
        };

        this._providers = new Map();
        
        const registerProvider = (type, selector, provider) => {
            const id = `${type}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
            this._providers.set(id, provider);
            this.ipcClient.sendNotification('languages/register', { type, selector, id });
            return { dispose: () => this._providers.delete(id) };
        };

        this.languages = {
            registerCompletionItemProvider: (selector, provider, ...triggerCharacters) => registerProvider('completion', selector, provider),
            registerHoverProvider: (selector, provider) => registerProvider('hover', selector, provider),
            registerDefinitionProvider: (selector, provider) => registerProvider('definition', selector, provider),
            registerDocumentFormattingEditProvider: (selector, provider) => registerProvider('formatting', selector, provider),
            getLanguages: async () => []
        };

        // Listen for requests from C++ core and dispatch to JS providers
        this.ipcClient.on('languages/request', async (payload) => {
            const { id, reqId, method, args } = payload;
            const provider = this._providers.get(id);
            if (!provider) {
                this.ipcClient.sendNotification('languages/response', { reqId, error: 'Provider not found' });
                return;
            }
            try {
                let result;
                // Basic mock doc until full document synchronization is implemented
                const doc = { uri: this.Uri.parse(args.uri || ''), getText: () => "" };
                
                if (method === 'provideCompletionItems' && provider.provideCompletionItems) {
                    const pos = new this.Position(args.position.line, args.position.character);
                    result = await provider.provideCompletionItems(doc, pos, { isCancellationRequested: false }, { triggerKind: 1 });
                } else if (method === 'provideHover' && provider.provideHover) {
                    const pos = new this.Position(args.position.line, args.position.character);
                    result = await provider.provideHover(doc, pos, { isCancellationRequested: false });
                } else if (method === 'provideDefinition' && provider.provideDefinition) {
                    const pos = new this.Position(args.position.line, args.position.character);
                    result = await provider.provideDefinition(doc, pos, { isCancellationRequested: false });
                }
                this.ipcClient.sendNotification('languages/response', { reqId, result });
            } catch (err) {
                this.ipcClient.sendNotification('languages/response', { reqId, error: err.message });
            }
        });

        this.env = {
            appName: 'Vrutti IDE',
            appRoot: '',
            language: 'en',
            clipboard: {
                readText: async () => "",
                writeText: async () => {}
            },
            openExternal: async (target) => {
                // stub
                return true;
            }
        };

        this.debug = {
            activeDebugSession: undefined,
            onDidStartDebugSession: () => ({ dispose: () => {} }),
            onDidTerminateDebugSession: () => ({ dispose: () => {} }),
            onDidChangeActiveDebugSession: () => ({ dispose: () => {} }),
            startDebugging: async () => false
        };

        this.extensions = {
            getExtension: () => undefined,
            all: [],
            onDidChange: () => ({ dispose: () => {} })
        };

        // Basic types
        this.Disposable = class Disposable {
            constructor(callOnDispose) {
                this.callOnDispose = callOnDispose;
            }
            dispose() {
                if (this.callOnDispose) {
                    this.callOnDispose();
                    this.callOnDispose = undefined;
                }
            }
            static from(...disposables) {
                return new Disposable(() => {
                    for (const d of disposables) {
                        if (d && typeof d.dispose === 'function') d.dispose();
                    }
                });
            }
        };

        this.EventEmitter = class EventEmitter {
            constructor() {
                this._listeners = new Set();
                this.event = (listener, thisArgs, disposables) => {
                    const bound = thisArgs ? listener.bind(thisArgs) : listener;
                    this._listeners.add(bound);
                    const disposable = {
                        dispose: () => this._listeners.delete(bound)
                    };
                    if (disposables) disposables.push(disposable);
                    return disposable;
                };
            }
            fire(data) {
                for (const listener of this._listeners) {
                    try { listener(data); } catch (e) { console.error(e); }
                }
            }
            dispose() {
                this._listeners.clear();
            }
        };

        this.Uri = class Uri {
            constructor(scheme, authority, path, query, fragment) {
                this.scheme = scheme;
                this.authority = authority;
                this.path = path;
                this.query = query;
                this.fragment = fragment;
            }
            static parse(value) { return new Uri('file', '', value, '', ''); }
            static file(path) { return new Uri('file', '', path, '', ''); }
            toString() { return this.scheme + '://' + this.path; }
        };

        this.Position = class Position {
            constructor(line, character) {
                this.line = line;
                this.character = character;
            }
        };

        this.Range = class Range {
            constructor(startLine, startCharacter, endLine, endCharacter) {
                if (arguments.length === 2) {
                    this.start = startLine;
                    this.end = startCharacter;
                } else {
                    this.start = new this.Position(startLine, startCharacter);
                    this.end = new this.Position(endLine, endCharacter);
                }
            }
        };

        this.Location = class Location {
            constructor(uri, rangeOrPosition) {
                this.uri = uri;
                this.range = rangeOrPosition;
            }
        };

        this.CompletionItem = class CompletionItem {
            constructor(label, kind) {
                this.label = label;
                this.kind = kind;
            }
        };

        this.Hover = class Hover {
            constructor(contents, range) {
                this.contents = contents;
                this.range = range;
            }
        };

        const apiSelf = this;
        class CancellationToken {
            constructor() {
                this._isCancelled = false;
                this._emitter = new apiSelf.EventEmitter();
            }
            get isCancellationRequested() { return this._isCancelled; }
            get onCancellationRequested() { return this._emitter.event; }
            _cancel() {
                this._isCancelled = true;
                this._emitter.fire();
            }
            _dispose() {
                this._emitter.dispose();
            }
        }

        this.CancellationTokenSource = class CancellationTokenSource {
            constructor() {
                this.token = new CancellationToken();
            }
            cancel() {
                this.token._cancel();
            }
            dispose() {
                this.token._dispose();
            }
        };
    }
}

module.exports = {
    createApi: (ipcClient) => new VruttiAPI(ipcClient)
};
