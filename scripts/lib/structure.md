# lib (scripts/lib/)

## Purpose
Helpers shared by the scripts: the test harness, init data and tracing preload.

## Files
- **exthost_harness.js**: Shared test harness for the Vrutti <-> VS Code Extension Host protocol.
- **init_data.js**: Builds the `IExtensionHostInitData` payload and `IExtensionDescription` objects that Vrutti sends to the VS...
- **TODO.md**: Open questions and planned work for this folder.
- **trace_preload.js**: Diagnostic preload for the Extension Host.

## Public Methods
- **ExtHostHarness()** (exported)
- **loadProxyIds()** (exported)
- **fileUri()** (exported)
- **REPO_ROOT()** (exported)
- **EXT_DIR()** (exported)
- **describeExtension()** (exported)
- **buildInitData()** (exported)
- **buildWorkspaceData()** (exported)
- **buildConfigurationInitData()** (exported)
- **stableUuid()** (exported)
