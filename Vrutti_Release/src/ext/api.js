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

        this.languages = {
            registerCompletionItemProvider: () => ({ dispose: () => {} }),
            registerHoverProvider: () => ({ dispose: () => {} }),
            registerDefinitionProvider: () => ({ dispose: () => {} }),
            registerDocumentFormattingEditProvider: () => ({ dispose: () => {} }),
            getLanguages: async () => []
        };

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
    }
}

module.exports = {
    createApi: (ipcClient) => new VruttiAPI(ipcClient)
};
