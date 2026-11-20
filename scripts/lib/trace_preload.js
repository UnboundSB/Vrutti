// Diagnostic preload for the Extension Host.
//
// Traces the bytes that cross the renderer <-> extension host socket so
// protocol problems can be diagnosed without touching the upstream VS Code
// tree.
//
// Enable it with:
//
//   VRUTTI_TRACE_BRIDGE=1
//   VRUTTI_TRACE_FILE=<path to a log file>
//
// The bootstrapper adds this file to the child process' `--require` list when
// `VRUTTI_TRACE_BRIDGE` is set. The trace is written to `VRUTTI_TRACE_FILE`
// (or a temporary file) instead of stdout so it cannot interfere with the
// protocol traffic itself.

'use strict';

if (!process.env.VRUTTI_TRACE_BRIDGE) {
    return;
}

const fs = require('fs');
const os = require('os');
const path = require('path');
const net = require('net');

const traceFile = process.env.VRUTTI_TRACE_FILE
    || path.join(os.tmpdir(), `vrutti-bridge-trace-${process.pid}.log`);

// NOTE: use append, not truncate -- the tsx loader may start more than one
// process and they all share this file.
fs.appendFileSync(traceFile, `[trace] pid=${process.pid}\n`);

function trace(direction, text) {
    if (!text) { return; }
    // Only the bridge protocol frames matter; the process also uses an IPC
    // channel (tsx loader messages) that would otherwise drown the trace.
    if (direction.startsWith('C:')) { /* console output is always relevant */ }
    else if (!text.includes('"type":') && !text.startsWith('RPC')) { return; }
    try {
        fs.appendFileSync(traceFile, `[${direction}] ${text.replace(/\n/g, '\\n')}\n`);
    } catch (_) { /* ignore */ }
}
const originalWrite = net.Socket.prototype.write;
net.Socket.prototype.write = function (chunk, ...rest) {
    try {
        const text = typeof chunk === 'string' ? chunk : Buffer.from(chunk).toString('utf8');
        if (text.length > 0) {
            trace('OUT', text.slice(0, 200000));
        }
    } catch (_) { /* ignore */ }
    return originalWrite.call(this, chunk, ...rest);
};

const originalOn = net.Socket.prototype.on;
net.Socket.prototype.on = function (event, listener) {
    if (event === 'data') {
        return originalOn.call(this, event, function (chunk) {
            try {
                const text = Buffer.from(chunk).toString('utf8');
                if (text.length > 0) {
                    trace('IN ', text.slice(0, 200000));
                }
            } catch (_) { /* ignore */ }
            return listener.apply(this, arguments);
        });
    }
    return originalOn.call(this, event, listener);
};

trace('SYS', `bridge tracing enabled -> ${traceFile}`);

// --- capture console output --------------------------------------------------
// The extension host reports protocol problems through console.error /
// console.log; capture them so they end up in the trace file.
for (const method of ['error', 'warn', 'log']) {
    const original = console[method].bind(console);
    console[method] = (...args) => {
        try {
            const text = args
                .map((a) => (a && a.stack) ? a.stack : (typeof a === 'string' ? a : safeInspect(a)))
                .join(' ');
            trace(`C:${method}`, text.slice(0, 1500));
        } catch (_) { /* ignore */ }
        return original(...args);
    };
}

function safeInspect(value) {
    try {
        return JSON.stringify(value);
    } catch (_) {
        return String(value);
    }
}

// --- intercept the upstream RPC protocol ------------------------------------
// The upstream VS Code tree is read-only, so the RPC layer is instrumented at
// runtime instead: `Module._load` lets us wrap the classes the extension host
// imports.
const Module = require('module');
const originalLoad = Module._load;

Module._load = function (request, parent, isMain) {
    const exported = originalLoad.apply(this, arguments);

    try {
        if (typeof request === 'string'
            && (request.includes('rpcProtocol') || request.includes('proxyIdentifier')
                || request.includes('ipc.net') || request.includes('extensionHostProcess'))) {
            trace('MOD ', `load ${request} -> ${Object.keys(exported || {}).slice(0, 12).join(',')}`);
        }
    } catch (_) { /* ignore */ }

    try {
        if (typeof request === 'string' && /rpcProtocol\.(js|cjs|mjs)$/.test(request)
            && exported && exported.RPCProtocol) {
            const proto = exported.RPCProtocol.prototype;

            const originalReceive = proto._receiveOneMessage;
            if (originalReceive && !originalReceive.__vruttiTraced) {
                const traced = function (rawmsg) {
                    trace('RPC ', `receive ${rawmsg ? rawmsg.byteLength : 0} bytes`);
                    try {
                        return originalReceive.call(this, rawmsg);
                    } catch (err) {
                        trace('RPC!', `receive threw: ${err && err.stack}`);
                        throw err;
                    }
                };
                traced.__vruttiTraced = true;
                proto._receiveOneMessage = traced;
            }

            const originalSend = proto.send;
            if (originalSend && !originalSend.__vruttiTraced) {
                const traced = function (msg) {
                    try {
                        trace('RPC>', `send ${msg && msg.byteLength} bytes`);
                    } catch (_) { /* ignore */ }
                    return originalSend.call(this, msg);
                };
                traced.__vruttiTraced = true;
                proto.send = traced;
            }
        }

        if (typeof request === 'string' && /proxyIdentifier\.(js|cjs|mjs)$/.test(request)
            && exported && exported.ProxyIdentifier) {
            trace('RPC=', `ProxyIdentifier module loaded (count=${exported.ProxyIdentifier.count})`);
        }
    } catch (_) { /* ignore */ }

    return exported;
};
