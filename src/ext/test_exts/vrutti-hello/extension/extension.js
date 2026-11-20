// Minimal extension used by scripts/test_exthost_activation.js.
//
// It performs the smallest set of `vscode.*` calls that proves the Extension
// Host really loaded and activated an extension:
//
//   * vscode.commands.registerCommand  -> MainThreadCommands.$registerCommand
//   * vscode.workspace.getConfiguration -> MainThreadConfiguration.$getConfiguration
//   * vscode.window.createOutputChannel -> MainThreadLogService.$createOutputChannel
//
// The activation test asserts that the renderer side receives the
// `$registerCommand` call for "vrutti.hello.sayHello".

const vscode = require('vscode');

function activate(context) {
    const channel = vscode.window.createOutputChannel('Vrutti Hello');
    context.subscriptions.push(channel);

    channel.appendLine('vrutti-hello activated');

    context.subscriptions.push(
        vscode.commands.registerCommand('vrutti.hello.sayHello', () => {
            channel.appendLine('hello from vrutti-hello');
            return 'hello';
        })
    );

    // Touch the configuration API so the renderer gets asked for settings.
    const config = vscode.workspace.getConfiguration('vrutti.hello');
    channel.appendLine(`config value: ${config.get('greeting', 'default')}`);

    return { greet: () => 'hello' };
}

function deactivate() {
    // Nothing to clean up.
}

module.exports = { activate, deactivate };
