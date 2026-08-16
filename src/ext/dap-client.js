const { spawn } = require('child_process');
const { EventEmitter } = require('events');
const fs = require('fs');

class DapClient extends EventEmitter {
    constructor(ipcClient) {
        super();
        this.ipcClient = ipcClient;
        this.process = null;
        this.seq = 1;
        this.pendingRequests = new Map();
        
        this.buffer = Buffer.alloc(0);
        this.contentLength = -1;
    }

    start(executable, args = [], cwd = process.cwd()) {
        if (this.process) return;

        this.process = spawn(executable, args, {
            cwd,
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true
        });

        this.process.stdout.on('data', (data) => this._handleData(data));
        
        this.process.stderr.on('data', (data) => {
            this.ipcClient.sendNotification('debug/log', { type: 'error', text: `[Adapter stderr]: ${data.toString()}` });
        });

        this.process.on('exit', (code) => {
            this.ipcClient.sendNotification('debug/log', { type: 'info', text: `Debug adapter exited with code ${code}` });
            this.process = null;
            this.emit('exited');
        });

        this.ipcClient.sendNotification('debug/log', { type: 'info', text: `Started debug adapter: ${executable} ${args.join(' ')}` });
    }

    stop() {
        if (this.process) {
            this.process.kill();
            this.process = null;
        }
    }

    sendRequest(command, args = {}) {
        return new Promise((resolve, reject) => {
            if (!this.process) return reject(new Error('Debug adapter not running'));
            
            const seq = this.seq++;
            const message = {
                seq,
                type: 'request',
                command,
                arguments: args
            };
            
            this.pendingRequests.set(seq, { resolve, reject });
            this._sendMessage(message);
        });
    }

    sendResponse(request, body = {}) {
        if (!this.process) return;
        const message = {
            seq: this.seq++,
            type: 'response',
            request_seq: request.seq,
            command: request.command,
            success: true,
            body
        };
        this._sendMessage(message);
    }

    sendErrorResponse(request, messageText) {
        if (!this.process) return;
        const message = {
            seq: this.seq++,
            type: 'response',
            request_seq: request.seq,
            command: request.command,
            success: false,
            message: messageText
        };
        this._sendMessage(message);
    }

    _sendMessage(message) {
        const json = JSON.stringify(message);
        const data = `Content-Length: ${Buffer.byteLength(json, 'utf8')}\r\n\r\n${json}`;
        if (this.process && this.process.stdin.writable) {
            this.process.stdin.write(data);
        }
    }

    _handleData(data) {
        this.buffer = Buffer.concat([this.buffer, data]);

        while (true) {
            if (this.contentLength >= 0) {
                if (this.buffer.length >= this.contentLength) {
                    const messageBuffer = this.buffer.slice(0, this.contentLength);
                    this.buffer = this.buffer.slice(this.contentLength);
                    this.contentLength = -1;

                    try {
                        const messageStr = messageBuffer.toString('utf8');
                        const message = JSON.parse(messageStr);
                        this._handleMessage(message);
                    } catch (err) {
                        console.error('Failed to parse DAP message:', err);
                    }
                } else {
                    break;
                }
            } else {
                const idx = this.buffer.indexOf('\r\n\r\n');
                if (idx !== -1) {
                    const header = this.buffer.slice(0, idx).toString('utf8');
                    this.buffer = this.buffer.slice(idx + 4);
                    
                    const match = header.match(/Content-Length: (\d+)/i);
                    if (match) {
                        this.contentLength = parseInt(match[1], 10);
                    } else {
                        console.error('Invalid DAP header (missing Content-Length)');
                        this.buffer = Buffer.alloc(0);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
    }

    _handleMessage(message) {
        if (message.type === 'response') {
            const req = this.pendingRequests.get(message.request_seq);
            if (req) {
                this.pendingRequests.delete(message.request_seq);
                if (message.success) {
                    req.resolve(message.body);
                } else {
                    req.reject(new Error(message.message || `Command ${message.command} failed`));
                }
            }
        } else if (message.type === 'event') {
            this.emit('event', message);
            this.emit(`event:${message.event}`, message.body);
        } else if (message.type === 'request') {
            this.emit('request', message);
        }
    }
}

module.exports = { DapClient };
