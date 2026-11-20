// Shared test harness for the Vrutti <-> VS Code Extension Host protocol.
//
// The harness plays the role of Vrutti's native C++ core: it opens the named
// pipe, launches the Extension Host through `src/ext/bootstrapper.js`, and
// speaks the "VruttiJSONBridge" protocol (newline delimited JSON).
//
// Wire format (see src/vr/workbench/api/node/extensionHostProcess.ts):
//
//   renderer -> host   { "type": "init",      "data": { ... } }
//                      { "type": "request",   "reqId": N, "proxyId": P,
//                                           "method": "M", "args": [ ... ] }
//                      { "type": "reply",     "reqId": N, "res": V }
//                      { "type": "replyErr",  "reqId": N, "err": { ... } }
//                      { "type": "ack",       "reqId": N }
//                      { "type": "cancel",    "reqId": N }
//                      { "type": "terminate" }
//
//   host -> renderer   { "type": "ready" }
//                      { "type": "request",   "reqId": N, "proxyId": P,
//                                           "method": "M", "args": [ ... ] }
//                      { "type": "reply",     "reqId": N, "res": V }
//                      { "type": "replyErr",  "reqId": N, "err": { ... } }

'use strict';

const net = require('net');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');

const REPO_ROOT = path.resolve(__dirname, '..', '..');
const EXT_DIR = path.join(REPO_ROOT, 'src', 'ext');
const PROXY_HEADER = path.join(REPO_ROOT, 'src', 'core', 'ipc', 'VsCodeProxyIds.generated.h');

/**
 * Loads the generated proxy-id table so the tests and the C++ core always
 * agree on the numeric `proxyId` of every RPC shape.
 */
function loadProxyIds() {
    const text = fs.readFileSync(PROXY_HEADER, 'utf8');
    const ids = Object.create(null);
    const re = /constexpr\s+int\s+([A-Za-z0-9_]+)\s*=\s*(\d+);/g;
    let match;
    while ((match = re.exec(text)) !== null) {
        ids[match[1]] = Number(match[2]);
    }
    if (Object.keys(ids).length === 0) {
        throw new Error(`no proxy ids parsed from ${PROXY_HEADER}`);
    }
    return ids;
}

/** Builds a VS Code style `file:` UriComponents object. */
function fileUri(p) {
    let norm = String(p).replace(/\\/g, '/');
    if (/^[a-zA-Z]:/.test(norm)) {
        norm = '/' + norm;
    }
    return { $mid: 1, scheme: 'file', path: norm };
}

class ExtHostHarness {
    /**
     * @param {object} options
     * @param {number} [options.timeoutMs]  overall test timeout
     * @param {boolean} [options.verbose]   log every frame
     */
    constructor(options = {}) {
        this.timeoutMs = options.timeoutMs ?? 60000;
        this.verbose = options.verbose ?? true;
        this.pipeName = `\\\\.\\pipe\\vrutti_test_${process.pid}_${Date.now()}`;
        this.proxyIds = loadProxyIds();

        this.server = null;
        this.socket = null;
        this.child = null;
        this.buffer = '';
        this.nextReqId = 1;

        /** @type {Array<object>} every frame received from the host */
        this.frames = [];
        /** @type {Array<object>} every `request` frame received from the host */
        this.hostRequests = [];
        /** @type {Map<number, {resolve: Function, reject: Function}>} */
        this.pending = new Map();
        /** @type {Map<string, Function[]>} */
        this.listeners = new Map();

        this.ready = false;
        this.exthostOutput = '';
        this.exthostErrors = '';
    }

    // -- event plumbing --------------------------------------------------------

    on(event, handler) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, []);
        }
        this.listeners.get(event).push(handler);
        return this;
    }

    emit(event, payload) {
        const handlers = this.listeners.get(event);
        if (!handlers) return;
        for (const handler of handlers.slice()) {
            try {
                handler(payload);
            } catch (e) {
                console.error(`[harness] listener for '${event}' threw:`, e);
            }
        }
    }

    /** Resolves once `predicate(frame)` is true for some received frame. */
    waitFor(predicate, timeoutMs = this.timeoutMs, label = 'frame') {
        const existing = this.frames.find(predicate);
        if (existing) return Promise.resolve(existing);

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.off('frame', onFrame);
                reject(new Error(`timed out after ${timeoutMs}ms waiting for ${label}`));
            }, timeoutMs);

            const onFrame = (frame) => {
                if (!predicate(frame)) return;
                clearTimeout(timer);
                this.off('frame', onFrame);
                resolve(frame);
            };

            this.on('frame', onFrame);
        });
    }

    off(event, handler) {
        const handlers = this.listeners.get(event);
        if (!handlers) return;
        const idx = handlers.indexOf(handler);
        if (idx !== -1) handlers.splice(idx, 1);
    }

    // -- lifecycle -------------------------------------------------------------

    start() {
        return new Promise((resolve, reject) => {
            this.server = net.createServer();

            this.server.on('error', (err) => {
                console.error('[harness] pipe server error:', err.message);
                reject(err);
            });

            this.server.listen(this.pipeName, () => {
                this.spawnExtensionHost();
                resolve();
            });

            this.server.on('connection', (socket) => {
                this.socket = socket;
                socket.setEncoding('utf8');
                socket.on('data', (chunk) => this.onData(chunk));
                socket.on('error', (err) => console.error('[harness] socket error:', err.message));
                socket.on('close', () => this.emit('close'));
            });
        });
    }

    spawnExtensionHost() {
        const bootstrapper = path.join(EXT_DIR, 'bootstrapper.js');
        this.child = spawn(process.execPath, [bootstrapper, `--pipe=${this.pipeName}`], {
            cwd: REPO_ROOT,
            stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
            env: { ...process.env },
        });

        this.child.stdout.on('data', (d) => {
            this.exthostOutput += d.toString();
            if (this.verbose) process.stdout.write(`[exthost] ${d}`);
        });
        this.child.stderr.on('data', (d) => {
            this.exthostErrors += d.toString();
            if (this.verbose) process.stderr.write(`[exthost-err] ${d}`);
        });
        this.child.on('exit', (code, signal) => {
            this.emit('exit', { code, signal });
        });
    }

    stop() {
        try { if (this.child) this.child.kill(); } catch (_) { /* ignore */ }
        try { if (this.socket) this.socket.destroy(); } catch (_) { /* ignore */ }
        try { if (this.server) this.server.close(); } catch (_) { /* ignore */ }
    }

    // -- framing ---------------------------------------------------------------

    onData(chunk) {
        this.buffer += chunk;
        let idx;
        while ((idx = this.buffer.indexOf('\n')) !== -1) {
            const line = this.buffer.slice(0, idx);
            this.buffer = this.buffer.slice(idx + 1);
            if (!line.trim()) continue;

            let frame;
            try {
                frame = JSON.parse(line);
            } catch (e) {
                console.error(`[harness] non-JSON frame: ${line.slice(0, 300)}`);
                continue;
            }

            this.frames.push(frame);
            if (this.verbose) {
                let summary;
                if (frame.type === 'request') {
                    summary = `${frame.proxyId}.${frame.method}`;
                    if (Array.isArray(frame.args) && frame.args.length > 0) {
                        let preview;
                        try {
                            preview = JSON.stringify(frame.args[0]);
                        } catch (_) {
                            preview = String(frame.args[0]);
                        }
                        if (preview && preview.length > 220) {
                            preview = preview.slice(0, 220) + '...';
                        }
                        summary += ` ${preview}`;
                    }
                } else if (frame.type === 'reply' || frame.type === 'replyErr') {
                    summary = `reqId=${frame.reqId}`;
                } else {
                    summary = '';
                }
                console.log(`[harness] << ${frame.type} ${summary}`);
            }
            this.emit('frame', frame);

            switch (frame.type) {
                case 'ready':
                    this.ready = true;
                    this.emit('ready', frame);
                    break;

                case 'request':
                    this.hostRequests.push(frame);
                    this.emit('request', frame);
                    // Default behaviour: acknowledge and answer so the host is
                    // never left waiting on an unanswered promise.
                    this.sendAck(frame.reqId);
                    if (!this.listeners.has('request')) {
                        this.sendReply(frame.reqId, null);
                    }
                    break;

                case 'reply': {
                    const pending = this.pending.get(frame.reqId);
                    if (pending) {
                        this.pending.delete(frame.reqId);
                        pending.resolve(frame.res);
                    }
                    this.emit('reply', frame);
                    break;
                }

                case 'replyErr': {
                    const pending = this.pending.get(frame.reqId);
                    if (pending) {
                        this.pending.delete(frame.reqId);
                        const err = frame.err || {};
                        pending.reject(new Error(err.message || 'extension host error'));
                    }
                    this.emit('replyErr', frame);
                    break;
                }

                case 'ack':
                    this.emit('ack', frame);
                    break;

                case 'cancel':
                    this.emit('cancel', frame);
                    break;

                default:
                    this.emit('other', frame);
                    break;
            }
        }
    }

    write(frame) {
        if (!this.socket) {
            throw new Error('harness: no socket -- the extension host has not connected yet');
        }
        if (this.verbose) {
            const summary = frame.type === 'request'
                ? `${frame.proxyId}.${frame.method}`
                : frame.type;
            console.log(`[harness] >> ${summary} (${JSON.stringify(frame).length} bytes)`);
        }
        this.socket.write(JSON.stringify(frame) + '\n');
    }

    // -- protocol --------------------------------------------------------------

    sendInit(data) {
        this.write({ type: 'init', data });
    }

    sendRequest(proxyId, method, args = [], timeoutMs = this.timeoutMs) {
        const reqId = this.nextReqId++;
        const promise = new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(reqId);
                reject(new Error(
                    `timed out after ${timeoutMs}ms waiting for a reply to ` +
                    `proxyId=${proxyId} method=${method} (reqId=${reqId})`
                ));
            }, timeoutMs);

            this.pending.set(reqId, {
                resolve: (v) => { clearTimeout(timer); resolve(v); },
                reject: (e) => { clearTimeout(timer); reject(e); },
            });
        });
        this.write({ type: 'request', reqId, proxyId, method, args });
        return { reqId, promise };
    }

    /** Convenience: send a request addressed by shape name. */
    sendShapeRequest(shapeName, method, args = []) {
        const proxyId = this.proxyIds[shapeName];
        if (proxyId === undefined) {
            throw new Error(`harness: unknown proxy shape '${shapeName}'`);
        }
        return this.sendRequest(proxyId, method, args);
    }

    sendReply(reqId, res) {
        this.write({ type: 'reply', reqId, res: res === undefined ? null : res });
    }

    sendReplyError(reqId, message, name = 'Error') {
        this.write({
            type: 'replyErr',
            reqId,
            err: { $isError: true, name, message, stack: `${name}: ${message}` },
        });
    }

    sendAck(reqId) {
        this.write({ type: 'ack', reqId });
    }

    sendCancel(reqId) {
        this.write({ type: 'cancel', reqId });
    }

    sendTerminate() {
        this.write({ type: 'terminate' });
    }
}

module.exports = { ExtHostHarness, loadProxyIds, fileUri, REPO_ROOT, EXT_DIR };
