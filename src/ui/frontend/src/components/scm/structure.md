# scm (src/ui/frontend/src/components/scm/)

## Purpose
Source control management: git integration, git graph visualization, diff viewing.

## Files
- **scmModel.ts**: The `SCMModel` module.
- **TODO.md**: Open questions and planned work for this folder.
- **vrutti-git-graph.ts**: The `VruttiGitGraph` module.
- **vrutti-scm.ts**: The `VruttiScm` module.

## Public Methods
- **SCMFile** (exported type)
- **SCMModel.parseGitStatus(statusOutput: string)**
- **VruttiGitGraph.updated(_changed: Map<string, any>)**
- **VruttiGitGraph.render()**
- **VruttiScm.connectedCallback()**
- **VruttiScm.disconnectedCallback()**
- **VruttiScm.render()**
