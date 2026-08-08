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
            }
        };

        this.workspace = {
            getRootPath: async () => {
                return this.ipcClient.sendRequest('workspace/getRootPath');
            },
            openTextDocument: async (uri) => {
                return this.ipcClient.sendRequest('workspace/openTextDocument', { uri });
            }
        };

        this._commandRegistry = new Map();
        
        this.commands = {
            registerCommand: (commandId, callback) => {
                this._commandRegistry.set(commandId, callback);
                return { dispose: () => this._commandRegistry.delete(commandId) };
            },
            executeCommand: async (commandId, ...args) => {
                const callback = this._commandRegistry.get(commandId);
                if (!callback) {
                    throw new Error(`Command '${commandId}' not found`);
                }
                return await callback(...args);
            }
        };
    }
}

module.exports = {
    createApi: (ipcClient) => new VruttiAPI(ipcClient)
};
