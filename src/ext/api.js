class VruttiAPI {
    constructor(ipcClient) {
        this.ipcClient = ipcClient;
        
        // This object acts as the polyfill for the extension API.
        // It uses a Linker approach: optionally relying on the native vrutti_bridge (N-API)
        // or falling back to the generic IPC.
        
        this.nativeLinker = null;
        try {
            // Attempt to link to the C++ core dynamically (N-API Addon)
            this.nativeLinker = require('./bridge/build/Release/vrutti_bridge.node');
        } catch (e) {
            // Fallback to standard IPC if native linker is not yet compiled
        }
        
        this.sendRequest = async (method, payload) => {
            if (this.nativeLinker && this.nativeLinker.sendRequestNative) {
                try {
                    const resultStr = this.nativeLinker.sendRequestNative(method, JSON.stringify(payload || {}));
                    const result = JSON.parse(resultStr);
                    if (result && result.status !== 'fallback') {
                        return result;
                    }
                } catch (e) { }
            }
            return this.ipcClient.sendRequest(method, payload);
        };

        this.sendNotification = (method, payload) => {
            if (this.nativeLinker && this.nativeLinker.sendNotificationNative) {
                try {
                    const resultStr = this.nativeLinker.sendNotificationNative(method, JSON.stringify(payload || {}));
                    const result = JSON.parse(resultStr);
                    if (result && result.status !== 'fallback') {
                        return;
                    }
                } catch (e) { }
            }
            this.ipcClient.sendNotification(method, payload);
        };
        
        this.window = {
            showInformationMessage: async (message) => {
                return this.sendRequest('window/showInformationMessage', { message });
            },
            showErrorMessage: async (message) => {
                return this.sendRequest('window/showErrorMessage', { message });
            },
            showWarningMessage: async (message) => {
                return this.sendRequest('window/showInformationMessage', { message: "[Warning] " + message });
            },
            setStatusBarMessage: (text, hideAfterTimeout) => {
                return { dispose: () => {} };
            },
            createOutputChannel: (name) => {
                return {
                    name,
                    append: (value) => this.sendNotification('run/output', { text: value }),
                    appendLine: (value) => this.sendNotification('run/output', { text: value + '\n' }),
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
                        this.sendNotification('terminal/runCommand', { command: text });
                    },
                    show: () => {},
                    hide: () => {},
                    dispose: () => {}
                };
            },
            createWebviewPanel: (viewType, title, showOptions, options) => {
                const id = `webview_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
                this.sendNotification('webview/createPanel', { id, viewType, title, showOptions, options });
                
                let html = '';
                const onDidReceiveMessageEmitter = new this.EventEmitter();
                
                this._webviews.set(id, { emitter: onDidReceiveMessageEmitter });
                
                const panel = {
                    webview: {
                        get html() { return html; },
                        set html(val) {
                            html = val;
                            this._ipcClient.sendNotification('webview/setHtml', { id, html: val });
                        },
                        onDidReceiveMessage: onDidReceiveMessageEmitter.event,
                        postMessage: async (msg) => {
                            this._ipcClient.sendNotification('webview/postMessage', { id, message: msg });
                            return true;
                        },
                    },
                    dispose: () => {
                        this._webviews.delete(id);
                        this.sendNotification('webview/dispose', { id });
                    }
                };
                // Bind internal ipc client for the getter/setter scope
                panel.webview._ipcClient = this.ipcClient;
                return panel;
            },
            registerWebviewViewProvider: (viewId, provider, options) => {
                this.sendNotification('webviewView/register', { viewId, options });
                this._webviewProviders.set(viewId, provider);
                return { 
                    dispose: () => {
                        this._webviewProviders.delete(viewId);
                        this.sendNotification('webviewView/unregister', { viewId });
                    }
                };
            }
        };

        this._webviews = new Map();
        this._webviewProviders = new Map();
        
        this.ipcClient.on('webviewView/resolve', async (payload) => {
            const { viewId, webviewId } = payload;
            const provider = this._webviewProviders.get(viewId);
            if (provider) {
                let html = '';
                const onDidReceiveMessageEmitter = new this.EventEmitter();
                this._webviews.set(webviewId, { emitter: onDidReceiveMessageEmitter });
                
                const webviewView = {
                    webview: {
                        get html() { return html; },
                        set html(val) {
                            html = val;
                            this._ipcClient.sendNotification('webview/setHtml', { id: webviewId, html: val });
                        },
                        onDidReceiveMessage: onDidReceiveMessageEmitter.event,
                        postMessage: async (msg) => {
                            this._ipcClient.sendNotification('webview/postMessage', { id: webviewId, message: msg });
                            return true;
                        },
                        _ipcClient: this.ipcClient
                    }
                };
                provider.resolveWebviewView(webviewView, {}, { isCancellationRequested: false });
            }
        });

        this.ipcClient.on('webview/receiveMessage', (payload) => {
            const { id, message } = payload;
            const webviewData = this._webviews.get(id);
            if (webviewData && webviewData.emitter) {
                webviewData.emitter.fire(message);
            }
        });

        let configurationCache = {};
        let configEmitter = null;
        
        this.ipcClient.on('workspace/didChangeConfiguration', (payload) => {
            configurationCache = payload.settings || {};
            if (configEmitter) configEmitter.fire({});
        });

        this.workspace = {
            getRootPath: async () => {
                return this.sendRequest('workspace/getRootPath');
            },
            openTextDocument: async (uri) => {
                return this.sendRequest('workspace/openTextDocument', { uri });
            },
            getConfiguration: (section, scope) => {
                const getNested = (obj, path) => {
                    return path.split('.').reduce((acc, part) => acc && acc[part], obj);
                };
                return {
                    get: (key, defaultValue) => {
                        const fullKey = section ? `${section}.${key}` : key;
                        const val = getNested(configurationCache, fullKey);
                        return val !== undefined ? val : defaultValue;
                    },
                    has: (key) => {
                        const fullKey = section ? `${section}.${key}` : key;
                        return getNested(configurationCache, fullKey) !== undefined;
                    },
                    inspect: (key) => undefined,
                    update: async (key, value, target) => {
                        const fullKey = section ? `${section}.${key}` : key;
                        return this.sendRequest('workspace/updateConfiguration', { key: fullKey, value, target });
                    }
                };
            },
            onDidChangeConfiguration: (listener, thisArgs, disposables) => {
                if (!configEmitter) configEmitter = new this.EventEmitter();
                return configEmitter.event(listener, thisArgs, disposables);
            },
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
            this.sendNotification('languages/register', { type, selector, id });
            return { dispose: () => this._providers.delete(id) };
        };

        this.languages = {
            registerCompletionItemProvider: (selector, provider, ...triggerCharacters) => registerProvider('completion', selector, provider),
            registerHoverProvider: (selector, provider) => registerProvider('hover', selector, provider),
            registerDefinitionProvider: (selector, provider) => registerProvider('definition', selector, provider),
            registerDocumentFormattingEditProvider: (selector, provider) => registerProvider('formatting', selector, provider),
            getLanguages: async () => [],
            createDiagnosticCollection: (name) => {
                const collectionName = name || `collection_${Date.now()}`;
                return {
                    name: collectionName,
                    set: (uri, diagnostics) => {
                        let entries = [];
                        if (Array.isArray(uri)) {
                            entries = uri;
                        } else {
                            entries = [[uri, diagnostics]];
                        }
                        const serialized = entries.map(([u, diags]) => ({
                            uri: u.toString(),
                            diagnostics: (diags || []).map(d => ({
                                range: d.range,
                                message: d.message,
                                severity: d.severity,
                                code: d.code,
                                source: d.source,
                                relatedInformation: d.relatedInformation
                            }))
                        }));
                        this.sendNotification('languages/diagnostics', {
                            collection: collectionName,
                            entries: serialized
                        });
                    },
                    delete: (uri) => {
                        this.sendNotification('languages/diagnostics/delete', {
                            collection: collectionName,
                            uri: uri.toString()
                        });
                    },
                    clear: () => {
                        this.sendNotification('languages/diagnostics/clear', { collection: collectionName });
                    },
                    dispose: () => {
                        this.sendNotification('languages/diagnostics/clear', { collection: collectionName });
                    }
                };
            }
        };

        // Listen for requests from C++ core and dispatch to JS providers
        this.ipcClient.on('languages/request', async (payload) => {
            const { id, reqId, method, args } = payload;
            const provider = this._providers.get(id);
            if (!provider) {
                this.sendNotification('languages/response', { reqId, error: 'Provider not found' });
                return;
            }
            try {
                let result;
                // Use the synchronized document from workspace
                let doc = this.workspace.textDocuments ? this.workspace.textDocuments.find(d => d.uri.toString() === args.uri) : null;
                if (!doc) {
                    doc = new this.TextDocument(args.uri || '', 'plaintext', 1, '');
                }
                
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
                this.sendNotification('languages/response', { reqId, result });
            } catch (err) {
                this.sendNotification('languages/response', { reqId, error: err.message });
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

        const self = this;
        this.lsp = {
            LanguageClient: class LanguageClient {
                constructor(id, name, serverOptions, clientOptions) {
                    this.id = id;
                    this.name = name;
                    this.serverOptions = serverOptions;
                    this.clientOptions = clientOptions;
                }
                
                start() {
                    const executable = this.serverOptions.run ? this.serverOptions.run.command : this.serverOptions.command;
                    const args = (this.serverOptions.run ? this.serverOptions.run.args : this.serverOptions.args) || [];
                    self.ipcClient.sendNotification('lsp/start', { executable, args, cwd: process.cwd() });
                }
                
                stop() {
                    self.ipcClient.sendNotification('lsp/stop', {});
                }
                
                async sendRequest(method, params) {
                    const id = Date.now() + Math.floor(Math.random() * 1000);
                    return new Promise((resolve, reject) => {
                        const handler = (payload) => {
                            if (payload.id === id) {
                                self.ipcClient.removeListener('lsp/response', handler);
                                if (payload.success) resolve(payload.result);
                                else reject(new Error(payload.error));
                            }
                        };
                        self.ipcClient.on('lsp/response', handler);
                        self.ipcClient.sendNotification('lsp/client_request', { method, params, id });
                    });
                }
                
                sendNotification(method, params) {
                    self.ipcClient.sendNotification('lsp/client_notification', { method, params });
                }
                
                onNotification(method, callback) {
                    self.ipcClient.on('lsp/notification', (msg) => {
                        if (msg.method === method) {
                            callback(msg.params);
                        }
                    });
                }
            }
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

        this.DiagnosticSeverity = {
            Error: 0,
            Warning: 1,
            Information: 2,
            Hint: 3
        };

        this.Diagnostic = class Diagnostic {
            constructor(range, message, severity) {
                this.range = range;
                this.message = message;
                this.severity = severity === undefined ? VruttiAPI.DiagnosticSeverity.Error : severity;
            }
        };

        this.DiagnosticRelatedInformation = class DiagnosticRelatedInformation {
            constructor(location, message) {
                this.location = location;
                this.message = message;
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

        // Setup Workspace Document Syncing
        const workspaceDocs = new Map();
        const onDidChangeDoc = new this.EventEmitter();
        const onDidOpenDoc = new this.EventEmitter();
        const onDidCloseDoc = new this.EventEmitter();

        this.TextDocument = class TextDocument {
            constructor(uri, languageId, version, text) {
                this.uri = typeof uri === 'string' ? apiSelf.Uri.parse(uri) : uri;
                this.languageId = languageId;
                this.version = version;
                this._text = text || "";
                this._lines = this._text.split(/\r?\n/);
            }
            get lineCount() { return this._lines.length; }
            getText(range) { return this._text; }
            positionAt(offset) {
                let currentOffset = 0;
                for (let i = 0; i < this._lines.length; i++) {
                    const lineLen = this._lines[i].length + 1;
                    if (currentOffset + lineLen > offset) {
                        return new apiSelf.Position(i, offset - currentOffset);
                    }
                    currentOffset += lineLen;
                }
                return new apiSelf.Position(this._lines.length - 1, (this._lines[this._lines.length - 1] || "").length);
            }
            offsetAt(position) {
                let offset = 0;
                for (let i = 0; i < position.line && i < this._lines.length; i++) {
                    offset += this._lines[i].length + 1;
                }
                return offset + position.character;
            }
            lineAt(lineOrPos) {
                const line = typeof lineOrPos === 'number' ? lineOrPos : lineOrPos.line;
                const text = this._lines[line] || "";
                return {
                    lineNumber: line,
                    text: text,
                    range: new apiSelf.Range(line, 0, line, text.length)
                };
            }
            _updateText(newText, version) {
                this._text = newText;
                this._lines = newText.split(/\r?\n/);
                this.version = version;
            }
        };

        this.workspace.textDocuments = [];
        this.workspace.onDidChangeTextDocument = onDidChangeDoc.event;
        this.workspace.onDidOpenTextDocument = onDidOpenDoc.event;
        this.workspace.onDidCloseTextDocument = onDidCloseDoc.event;

        this.ipcClient.on('workspace/didOpenTextDocument', (payload) => {
            const doc = new this.TextDocument(payload.uri, payload.languageId, payload.version, payload.text);
            workspaceDocs.set(payload.uri, doc);
            this.workspace.textDocuments = Array.from(workspaceDocs.values());
            onDidOpenDoc.fire(doc);
        });

        this.ipcClient.on('workspace/didChangeTextDocument', (payload) => {
            const doc = workspaceDocs.get(payload.uri);
            if (doc) {
                doc._updateText(payload.text, payload.version);
                onDidChangeDoc.fire({
                    document: doc,
                    contentChanges: [{ text: payload.text }]
                });
            }
        });

        this.ipcClient.on('workspace/didCloseTextDocument', (payload) => {
            const doc = workspaceDocs.get(payload.uri);
            if (doc) {
                workspaceDocs.delete(payload.uri);
                this.workspace.textDocuments = Array.from(workspaceDocs.values());
                onDidCloseDoc.fire(doc);
            }
        });
    }
}

module.exports = {
    createApi: (ipcClient) => new VruttiAPI(ipcClient)
};
