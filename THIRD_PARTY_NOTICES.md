# Third party notices

Vrutti is distributed under the MIT license. See `LICENSE` for the full text.

Vrutti links against and redistributes parts of other projects. The notices
below are the attribution those projects require.

---

## Microsoft Visual Studio Code — extension host service layer

The extension host that ships with Vrutti is built from the extension host
service layer of the Visual Studio Code source tree (`src/vr/workbench/api/**`
and the modules it depends on). Vrutti does not copy that code into its own
source tree: `src/ext/exthost/extensionHostProcess.ts` is original Vrutti
code that imports the upstream modules, and `scripts/build_exthost.js` bundles
them into a single file for the release.

The upstream modules are covered by the MIT license:

```
MIT License

Copyright (c) Microsoft Corporation. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The source of the bundled service layer is the Visual Studio Code repository
at <https://github.com/microsoft/vscode>.

Two changes are needed to make the service layer talk to the Vrutti core
instead of the Electron workbench, and both of them live in Vrutti's own
entry point:

* the transport is a newline delimited JSON dialect (`VruttiJSONBridge`)
  instead of the binary `PersistentProtocol` framing, and
* the start-up handshake is the one the Vrutti core speaks: init data,
  configuration, workspace, startup activation and command registration.

---

## Node.js

The release ships a Node.js runtime (`src/ext/bin/node.exe`) to run the
extension host. Node.js is distributed under the MIT license and is a
trademark of the OpenJS Foundation. See <https://nodejs.org/en/license>.

---

## Bundled npm packages

The extension host bundle includes the following packages, each under the MIT
license:

| Package | Copyright |
| ------- | --------- |
| `@vscode/proxy-agent` | Copyright (c) Microsoft Corporation |
| `@vscode/windows-process-tree` | Copyright (c) Microsoft Corporation |
| `@vscode/spdlog` | Copyright (c) Microsoft Corporation |
| `vscode-oniguruma` | Copyright (c) Microsoft Corporation |
| `vscode-textmate` | Copyright (c) Microsoft Corporation |
| `kerberos` | Copyright (c) MIT |
| `yauzl` | Copyright (c) Isaac Z. Schlueter and contributors |
| `yazl` | Copyright (c) Isaac Z. Schlueter and contributors |

The full license text of the MIT license is reproduced above. The copyright
notices of the individual packages are kept in the bundle: the build uses
`legalComments: 'inline'`, so they are part of `extensionHostProcess.mjs`.

---

## Dear ImGui

The native user interface uses Dear ImGui, distributed under the MIT license.
Copyright (c) 2017-2025 Omar Cornut. See <https://github.com/ocornut/imgui>.
