const net = require('net');
const { EventEmitter } = require('events');

class IPCClient extends EventEmitter {
    constructor(pipeName) {
        super();
        this.pipeName = pipeName;
        this.client = null;
        this.messageBuffer = '';
        this.messageId = 0;
        this.pendingRequests = new Map();
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.client = net.createConnection(this.pipeName, () => {
                console.log(`[IPC] Connected to native core via ${this.pipeName}`);
                resolve();
            });

            this.client.on('data', (data) => this._handleData(data));
            
            this.client.on('error', (err) => {
                console.error('[IPC] Connection error:', err);
                reject(err);
            });

            this.client.on('end', () => {
                console.log('[IPC] Disconnected from native core');
                process.exit(0);
            });
        });
    }

    sendRequest(method, params = {}) {
        return new Promise((resolve, reject) => {
            const id = ++this.messageId;
            this.pendingRequests.set(id, { resolve, reject });
            
            const payload = JSON.stringify({
                jsonrpc: '2.0',
                id,
                method,
                params
            }) + '\n'; // newline delimited JSON

            this.client.write(payload);
        });
    }

    sendNotification(method, params = {}) {
        const payload = JSON.stringify({
            jsonrpc: '2.0',
            method,
            params
        }) + '\n';
        
        this.client.write(payload);
    }

    _handleData(data) {
        this.messageBuffer += data.toString('utf8');
        let newlineIndex;
        
        while ((newlineIndex = this.messageBuffer.indexOf('\n')) !== -1) {
            const line = this.messageBuffer.slice(0, newlineIndex);
            this.messageBuffer = this.messageBuffer.slice(newlineIndex + 1);
            
            if (line.trim()) {
                try {
                    this._processMessage(JSON.parse(line));
                } catch (e) {
                    console.error('[IPC] Failed to parse message:', e, line);
                }
            }
        }
    }

    _processMessage(msg) {
        if (msg.id !== undefined && this.pendingRequests.has(msg.id)) {
            const { resolve, reject } = this.pendingRequests.get(msg.id);
            this.pendingRequests.delete(msg.id);
            
            if (msg.error) {
                reject(new Error(msg.error.message || 'Unknown RPC error'));
            } else {
                resolve(msg.result);
            }
        } else if (msg.method) {
            // It's a notification or request from C++
            this.emit(msg.method, msg.params);
        }
    }
}

module.exports = IPCClient;
