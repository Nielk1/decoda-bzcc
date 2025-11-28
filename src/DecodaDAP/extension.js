const vscode = require('vscode');
const { DebugAdapterExecutableFactory } = require('vscode');
function activate(context) {
    console.log("Decoda extension activated");

    context.subscriptions.push(
        vscode.debug.registerDebugAdapterDescriptorFactory('decoda', {
            createDebugAdapterDescriptor(session) {
                console.log("Decoda createDebugAdapterDescriptor");
                const config = session.configuration;
                // Log the env for debugging (optional)
                //if (config.env) {
                //    console.log("Environment variables:", config.env);
                //}
                //if (config.host && config.port) {
                //    // Connect to remote DAP server
                //    return new vscode.DebugAdapterServer(config.port, config.host);
                //} else {
                // Fallback: launch local DAP executable (if you want)
                const path = require('path');
                //const dapExe = path.join(context.extensionPath, 'decoda-dap.exe');
                const dapExe = path.join(context.extensionPath, 'DecodaDAP.exe');
                console.log("Launching DAP at:", dapExe);
                return new vscode.DebugAdapterExecutable(dapExe, []);
                //}
            }
        })
    );
}

function deactivate() { }

module.exports = { activate, deactivate };