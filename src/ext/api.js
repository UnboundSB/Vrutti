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
    }
}

module.exports = {
    createApi: (ipcClient) => new VruttiAPI(ipcClient)
};
