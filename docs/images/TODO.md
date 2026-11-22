# Vrutti IDE — docs/images/ TODO

**Scope:** `vrutti_ide/docs/images/` only. Parent `vrutti/` never touched.

## Purpose
The artwork of the project: the banner, the product mockup and the diagrams
that the README shows. Everything here is hand-drawn vector art, so the text
stays sharp at any size and the files stay small.

## Files
| File | Purpose |
|------|---------|
| `hero.svg` | The banner at the top of the README: the tree of circuitry, the mark of Vrutti |
| `editor.svg` | The workbench mockup: activity bar, explorer, tabs, editor, terminal, status bar |
| `architecture.svg` | The architecture diagram: frontend, bridge, extension host above the native core |
| `extensions.svg` | The extension bridge diagram: how an extension reaches the native core |
| `structure.md` | The generated list of these files |

## Tasks (Priority Order)
- [x] **IMG1.** The banner. The mark of Vrutti, drawn in the brand gradient.
- [x] **IMG2.** The workbench mockup, in the palette the app actually uses.
- [x] **IMG3.** The architecture diagram.
- [x] **IMG4.** The extension bridge diagram.
- [ ] **IMG5.** A short animated loop of the banner, for the repository page.
- [ ] **IMG6.** Mockups of the other workbench surfaces: the extension gallery,
      the settings editor, the Git graph, the diff editor.
- [ ] **IMG7.** A PNG export of each drawing, for the places that cannot show
      vector art.

## Rules
- Every file is valid XML. A raw angle bracket inside a text node breaks the
  render, so it is escaped.
- The colors are the palette of the app: `#1a1b26`, `#1e1e1e`, `#23273b`,
  `#3b4261`, `#7aa2f7`, `#f7768e`, `#9ece6a`, `#e0af68`, `#c0caf5`.
- Nothing in the artwork says something the code does not do. The diagrams
  describe the real architecture, not the planned one.

## Related
- `../README.md` — where these files are shown
- `../../src/ui/frontend/src/main.ts` — the palette of the workbench
