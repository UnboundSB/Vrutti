/*---------------------------------------------------------------------------------------------
 *  Vrutti IDE -- Extension Host entry point.
 *
 *  Copyright (c) Vrutti IDE contributors.
 *  Licensed under the MIT License. See LICENSE in the repository root.
 *
 *  This file contains original Vrutti code. It does not copy upstream source;
 *  it links against the extension-host service modules of the MIT licensed
 *  VS Code source tree (see THIRD_PARTY_NOTICES.md for the required notice).
 *--------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------
 *  WHY VRUTTI NEEDS ITS OWN ENTRY POINT
 *  ------------------------------------
 *  Vrutti talks to the Extension Host over a newline-delimited JSON dialect
 *  ("VruttiJSONBridge") instead of the binary `PersistentProtocol` framing.
 *  The bridge implementation lives in the VS Code source tree at
 *  `src/vr/workbench/api/node/extensionHostProcess.ts`, where it is patched for
 *  Vrutti.  That patch has two fatal defects:
 *
 *    1. `MessageType.Initialized` is encoded as the single byte 1, which is
 *       also `RPCMessageType.RequestJSONArgs`.  The bridge's `send()` therefore
 *       mistakes the "I am initialized" control frame for an RPC request, tries
 *       to deserialize it as one, throws, and the exception is swallowed by the
 *       frame parser.  `connectToRenderer()` never resolves, the extension host
 *       main object is never constructed and *no* extension can ever activate.
 *
 *    2. Outgoing frames are decoded with `new MessageBuffer(msg, 0)`, i.e.
 *       starting at the RPC type byte instead of after the 5 byte
 *       type + request-id header.  Every host -> renderer message is garbage.
 *
 *  The VS Code tree is read-only for this project, so Vrutti ships its own
 *  entry point: the same upstream services, but with a correct bridge.
 *--------------------------------------------------------------------------------------------*/

import * as net from 'net';

import { ErrorCode, formatError } from '../errorCodes';

import { VSBuffer } from '../../../../src/vr/base/common/buffer.js';
import { isCancellationError, isSigPipeError, onUnexpectedError } from '../../../../src/vr/base/common/errors.js';
import type { IURITransformer } from '../../../../src/vr/base/common/uriIpc.js';
import { createURITransformer } from '../../../../src/vr/base/common/uriTransformer.js';
import type { IMessagePassingProtocol } from '../../../../src/vr/base/parts/ipc/common/ipc.js';
import { BufferedEmitter } from '../../../../src/vr/base/parts/ipc/common/ipc.net.js';
import { Promises } from '../../../../src/vr/base/node/pfs.js';
import type { IHostUtils } from '../../../../src/vr/workbench/api/common/extHostExtensionService.js';
import { ExtensionHostMain } from '../../../../src/vr/workbench/api/common/extensionHostMain.js';
import type { IExtensionHostInitData } from '../../../../src/vr/workbench/services/extensions/common/extensionHostProtocol.js';
import { MessageBuffer, MessageIO } from '../../../../src/vr/workbench/services/extensions/common/rpcProtocol.js';

import '../../../../src/vr/workbench/api/common/extHost.common.services.js';
import '../../../../src/vr/workbench/api/node/extHost.node.services.js';

// -----------------------------------------------------------------------------
// Wire constants
// -----------------------------------------------------------------------------

/**
 * Control frames. Upstream encodes them as a single byte; the renderer side
 * never sees them because they are consumed by the bridge itself.
 *
 * NOTE: byte 1 is *also* `RPCMessageType.RequestJSONArgs`.  A control frame is
 * always exactly one byte long while an RPC frame always carries a 5 byte
 * header (type + 32 bit request id) plus a payload, so the length is what
 * disambiguates the two.  Getting this wrong is what broke extension
 * activation in the upstream bridge.
 */
const CONTROL_INITIALIZED = 1;
const CONTROL_READY = 2;
const CONTROL_TERMINATE = 3;

/** RPC message types (mirrors `RPCMessageType` in the upstream rpc protocol). */
const RPC_REQUEST_JSON_ARGS = 1;
const RPC_REQUEST_JSON_ARGS_WITH_CANCELLATION = 2;
const RPC_REQUEST_MIXED_ARGS = 3;
const RPC_REQUEST_MIXED_ARGS_WITH_CANCELLATION = 4;
const RPC_ACKNOWLEDGED = 5;
const RPC_CANCEL = 6;
const RPC_REPLY_OK_EMPTY = 7;
const RPC_REPLY_OK_VS_BUFFER = 8;
const RPC_REPLY_OK_JSON = 9;
const RPC_REPLY_OK_JSON_WITH_BUFFERS = 10;
const RPC_REPLY_ERR_ERROR = 11;
const RPC_REPLY_ERR_EMPTY = 12;

/** Property name used to carry a binary payload through JSON. */
const VS_BUFFER_MARKER = '$vsbuffer';

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

function log(message: string): void {
	if (process.env.VRUTTI_EXTHOST_LOG) {
		process.stderr.write(`[VruttiExtHost] ${message}\n`);
	}
}

function nativeExit(code = 0): void {
	process.exit(code);
}

/** Builds a one byte control frame. */
function controlFrame(byte: number): VSBuffer {
	const buffer = VSBuffer.alloc(1);
	buffer.writeUInt8(byte, 0);
	return buffer;
}

/** Encodes a binary payload so it survives a JSON round trip. */
function encodeVSBuffer(buffer: VSBuffer): { [VS_BUFFER_MARKER]: string } {
	return { [VS_BUFFER_MARKER]: Buffer.from(buffer.buffer).toString('base64') };
}

/**
 * Reverses {@link encodeVSBuffer} for every value in a JSON tree.
 *
 * The renderer sends binary payloads as `{ "$vsbuffer": "<base64>" }`; those
 * have to become real `VSBuffer` instances again, otherwise the upstream
 * argument serializer would treat them as plain objects.
 */
function reviveVSBuffers(value: any): any {
	if (Array.isArray(value)) {
		for (let i = 0; i < value.length; i++) {
			value[i] = reviveVSBuffers(value[i]);
		}
		return value;
	}
	if (value && typeof value === 'object') {
		const marker = (value as any)[VS_BUFFER_MARKER];
		if (typeof marker === 'string') {
			return VSBuffer.wrap(Buffer.from(marker, 'base64'));
		}
		for (const key of Object.keys(value)) {
			value[key] = reviveVSBuffers(value[key]);
		}
	}
	return value;
}

// -----------------------------------------------------------------------------
// The bridge
// -----------------------------------------------------------------------------

interface BridgeFrame {
	type: string;
	[key: string]: unknown;
}

/**
 * Translates between the newline-delimited JSON wire format and the
 * `IMessagePassingProtocol` the upstream RPC layer expects.
 */
class VruttiJSONBridge implements IMessagePassingProtocol {

	private readonly _onMessage = new BufferedEmitter<VSBuffer>();
	readonly onMessage = this._onMessage.event;

	private readonly _socket: net.Socket;
	private _pending = '';
	private _terminating = false;

	private _resolveInit!: (data: IExtensionHostInitData) => void;
	private _rejectInit!: (err: Error) => void;
	private readonly _initData: Promise<IExtensionHostInitData>;

	private _terminateHandler: (reason: string) => void = () => nativeExit();

	constructor(socket: net.Socket) {
		this._socket = socket;
		socket.setNoDelay(true);
		socket.on('data', (data: Buffer) => this._onData(data));
		socket.on('close', () => {
			this._terminateHandler('renderer closed the socket');
		});
		socket.on('error', (err: Error) => {
			log(`socket error: ${err.message}`);
		});

		this._initData = new Promise<IExtensionHostInitData>((resolve, reject) => {
			this._resolveInit = resolve;
			this._rejectInit = reject;
		});
	}

	/** Resolves with the initialization payload sent by the renderer. */
	get initData(): Promise<IExtensionHostInitData> {
		return this._initData;
	}

	onTerminate(handler: (reason: string) => void): void {
		this._terminateHandler = handler;
	}

	// -- renderer -> host --------------------------------------------------------

	private _onData(data: Buffer): void {
		this._pending += data.toString('utf8');

		let newline = this._pending.indexOf('\n');
		while (newline !== -1) {
			const line = this._pending.substring(0, newline);
			this._pending = this._pending.substring(newline + 1);
			if (line.trim().length > 0) {
				this._handleLine(line);
			}
			newline = this._pending.indexOf('\n');
		}
	}

	private _handleLine(line: string): void {
		let frame: BridgeFrame;
		try {
			frame = JSON.parse(line) as BridgeFrame;
		} catch (err) {
			log(formatError(ErrorCode.IpcFrameDecodeFailed, `incoming frame: ${err}`));
			return;
		}

		try {
			this._handleFrame(frame);
		} catch (err) {
			log(formatError(ErrorCode.IpcProtocolMismatch, `'${frame.type}' frame: ${err}`));
		}
	}

	private _handleFrame(frame: BridgeFrame): void {
		switch (frame.type) {
			case 'init': {
				const data = frame.data as IExtensionHostInitData;
				if (!data || typeof data !== 'object') {
					this._rejectInit(new Error(formatError(ErrorCode.IpcHandshakeFailed,
						'the init frame carries no data object')));
					return;
				}
				this._resolveInit(data);
				return;
			}

			case 'request': {
				const reqId = Number(frame.reqId);
				const proxyId = Number(frame.proxyId);
				const method = String(frame.method);
				const rawArgs = Array.isArray(frame.args) ? frame.args : [];
				const args = reviveVSBuffers(rawArgs) as any[];
				const serialized = MessageIO.serializeRequestArguments(args, null);
				const buffer = MessageIO.serializeRequest(
					reqId,
					proxyId,
					method,
					serialized,
					frame.cancellation === true
				);
				this._onMessage.fire(buffer);
				return;
			}

			case 'reply': {
				const reqId = Number(frame.reqId);
				const result = reviveVSBuffers(frame.res) as any;
				this._onMessage.fire(MessageIO.serializeReplyOK(reqId, result, null));
				return;
			}

			case 'replyErr': {
				const reqId = Number(frame.reqId);
				this._onMessage.fire(MessageIO.serializeReplyErr(reqId, frame.err as any));
				return;
			}

			case 'ack': {
				this._onMessage.fire(MessageIO.serializeAcknowledged(Number(frame.reqId)));
				return;
			}

			case 'cancel': {
				this._onMessage.fire(MessageIO.serializeCancel(Number(frame.reqId)));
				return;
			}

			case 'terminate': {
				this._terminating = true;
				this._terminateHandler('received terminate message from renderer');
				return;
			}

			default:
				log(`ignoring unknown frame type '${frame.type}'`);
		}
	}

	// -- host -> renderer --------------------------------------------------------

	/**
	 * Serializes an outgoing RPC message.
	 *
	 * The upstream implementation decoded the payload starting at offset 0,
	 * which included the RPC type byte and therefore produced garbage.  The
	 * correct sequence is: type byte, then the 32 bit request id, then the
	 * payload -- exactly like the upstream RPC reader does.
	 */
	send(message: VSBuffer): void {
		if (this._terminating) {
			return;
		}
		if (!message || message.byteLength === 0) {
			return;
		}

		// Control frames are always exactly one byte long.
		if (message.byteLength === 1) {
			const control = message.readUInt8(0);
			switch (control) {
				case CONTROL_READY:
					this._write({ type: 'ready' });
					return;
				case CONTROL_INITIALIZED:
					// The renderer learns about initialisation from the `ready`
					// frame it already received; nothing to send back.
					return;
				case CONTROL_TERMINATE:
					this._write({ type: 'terminated' });
					return;
				default:
					log(`ignoring unknown control frame ${control}`);
					return;
			}
		}

		const buffer = MessageBuffer.read(message, 0);
		const type = buffer.readUInt8();
		const reqId = buffer.readUInt32();

		switch (type) {
			case RPC_REQUEST_JSON_ARGS:
			case RPC_REQUEST_JSON_ARGS_WITH_CANCELLATION: {
				const parsed = MessageIO.deserializeRequestJSONArgs(buffer);
				this._write({
					type: 'request',
					reqId,
					proxyId: parsed.rpcId,
					method: parsed.method,
					args: parsed.args,
					cancellation: type === RPC_REQUEST_JSON_ARGS_WITH_CANCELLATION,
				});
				return;
			}

			case RPC_REQUEST_MIXED_ARGS:
			case RPC_REQUEST_MIXED_ARGS_WITH_CANCELLATION: {
				const parsed = MessageIO.deserializeRequestMixedArgs(buffer);
				this._write({
					type: 'request',
					reqId,
					proxyId: parsed.rpcId,
					method: parsed.method,
					args: parsed.args,
					mixed: true,
					cancellation: type === RPC_REQUEST_MIXED_ARGS_WITH_CANCELLATION,
				});
				return;
			}

			case RPC_REPLY_OK_EMPTY:
				this._write({ type: 'reply', reqId, res: null });
				return;

			case RPC_REPLY_OK_JSON:
				this._write({ type: 'reply', reqId, res: MessageIO.deserializeReplyOKJSON(buffer) });
				return;

			case RPC_REPLY_OK_VS_BUFFER:
				this._write({
					type: 'reply',
					reqId,
					res: encodeVSBuffer(MessageIO.deserializeReplyOKVSBuffer(buffer)),
				});
				return;

			case RPC_REPLY_OK_JSON_WITH_BUFFERS: {
				const withBuffers = MessageIO.deserializeReplyOKJSONWithBuffers(buffer, null);
				this._write({ type: 'reply', reqId, res: withBuffers.value });
				return;
			}

			case RPC_REPLY_ERR_EMPTY:
				this._write({ type: 'replyErr', reqId, err: null });
				return;

			case RPC_REPLY_ERR_ERROR:
				this._write({ type: 'replyErr', reqId, err: MessageIO.deserializeReplyErrError(buffer) });
				return;

			case RPC_ACKNOWLEDGED:
				this._write({ type: 'ack', reqId });
				return;

			case RPC_CANCEL:
				this._write({ type: 'cancel', reqId });
				return;

			default:
				log(`ignoring unknown RPC message type ${type}`);
		}
	}

	async drain(): Promise<void> {
		// Nothing is queued on the socket: every frame is written immediately.
	}

	/** Writes a raw frame to the renderer. */
	_write(frame: BridgeFrame): void {
		if (this._terminating) {
			return;
		}
		try {
			this._socket.write(JSON.stringify(frame) + '\n');
		} catch (err) {
			log(formatError(ErrorCode.IpcWriteFailed, `frame: ${err}`));
		}
	}
}

// -----------------------------------------------------------------------------
// Bootstrap
// -----------------------------------------------------------------------------

function connect(pipeName: string): Promise<net.Socket> {
	return new Promise<net.Socket>((resolve, reject) => {
		const socket = net.createConnection(pipeName, () => {
			socket.removeListener('error', reject);
			resolve(socket);
		});
		socket.once('error', reject);
	});
}

async function run(): Promise<void> {

	const pipeName = process.env.VSCODE_EXTHOST_IPC_HOOK;
	if (!pipeName) {
		throw new Error(formatError(ErrorCode.IpcHostNotReady,
			'VSCODE_EXTHOST_IPC_HOOK is not set; the extension host cannot connect to the renderer.'));
	}

	let socket: net.Socket;
	try {
		socket = await connect(pipeName);
	} catch (err) {
		throw new Error(formatError(ErrorCode.IpcConnectionFailed, `pipe ${pipeName}: ${err}`));
	}
	const bridge = new VruttiJSONBridge(socket);

	// Let the launcher know the transport is up.
	process.send?.({ type: 'VSCODE_EXTHOST_IPC_READY' });

	// Announce readiness; the renderer answers with the init payload.
	bridge.send(controlFrame(CONTROL_READY));

	const initData = await bridge.initData;

	// Host abstraction.
	const hostUtils = new class NodeHost implements IHostUtils {
		declare readonly _serviceBrand: undefined;
		public readonly pid = process.pid;
		public exit(code: number): void { nativeExit(code); }
		public fsExists(path: string): Promise<boolean> { return Promises.exists(path); }
		public fsRealpath(path: string): Promise<string> { return Promises.realpath(path); }
	};

	// A remote authority is only set for remote workspaces, which Vrutti does
	// not support yet, but the plumbing is kept so the behaviour matches.
	let uriTransformer: IURITransformer | null = null;
	if (initData.remote?.authority) {
		uriTransformer = createURITransformer(initData.remote.authority);
	}

	const environment = initData.environment as any;
	if (environment) {
		environment.useHostProxy = environment.useHostProxy ?? undefined;
		environment.skipWorkspaceStorageLock = environment.skipWorkspaceStorageLock ?? false;
	}

	const extensionHostMain = new ExtensionHostMain(
		bridge,
		initData,
		hostUtils,
		uriTransformer
	);

	bridge.onTerminate((reason: string) => extensionHostMain.terminate(reason));

	log('extension host main started');
}

process.on('unhandledRejection', (reason: unknown) => {
	if (!isCancellationError(reason)) {
		onUnexpectedError(reason);
	}
});

process.on('uncaughtException', (err: Error) => {
	if (!isSigPipeError(err)) {
		onUnexpectedError(err);
	}
});

run().catch((err: unknown) => {
	log(formatError(ErrorCode.IpcHostCrashed, err instanceof Error ? (err.stack ?? err.message) : String(err)));
	nativeExit(1);
});
