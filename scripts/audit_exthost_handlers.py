#!/usr/bin/env python3
"""Audits the extension host contract.

The extension host calls the renderer through the `MainThread*` shapes of the
upstream protocol. Every one of those calls that Vrutti does not answer is
an extension that misbehaves, so the gap between the two lists is the
shortest distance between "works in the reference editor" and "works in
Vrutti".

This script compares:

  * the methods the upstream service layer can call, taken from the protocol
    definition of the upstream source tree, and
  * the handlers Vrutti registers, taken from `VSCodeProtocol.cpp`.

Usage:

    python scripts/audit_exthost_handlers.py            # print the report
    python scripts/audit_exthost_handlers.py --check    # fail on a gap
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

PROTOCOL_CPP = REPO_ROOT / "src" / "core" / "ipc" / "VSCodeProtocol.cpp"

# The upstream protocol definition. The tree renames `vs` to `vr`.
UPSTREAM_PROTOCOL_CANDIDATES = [
    REPO_ROOT.parent / "src" / "vr" / "workbench" / "api" / "common" / "extHost.protocol.ts",
    REPO_ROOT.parent / "src" / "vs" / "workbench" / "api" / "common" / "extHost.protocol.ts",
]

HANDLER_RE = re.compile(r'm_handlers\["(?P<key>[^"]+)"\]')


def match_braces(text: str, open_index: int) -> int:
    """Returns the index of the brace that closes the one at `open_index`."""
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    return -1


def main_context_shapes(text: str) -> dict[str, str]:
    """Returns {proxy key: interface name} of the MainContext registry."""
    start = text.find("export const MainContext")
    if start == -1:
        return {}
    open_brace = text.find("{", start)
    close_brace = match_braces(text, open_brace)
    body = text[open_brace:close_brace]
    shapes: dict[str, str] = {}
    for match in re.finditer(
            r"(?P<key>[A-Za-z_$][\w$]*)\s*:\s*createProxyIdentifier\s*<"
            r"(?P<iface>[A-Za-z_$][\w$]*)>", body):
        shapes[match.group("key")] = match.group("iface")
    return shapes


def interface_methods(text: str, name: str) -> list[str]:
    """Returns the `$` methods of an interface of the protocol file."""
    match = re.search(rf"export\s+interface\s+{re.escape(name)}\b", text)
    if not match:
        return []
    open_brace = text.find("{", match.end())
    if open_brace == -1:
        return []
    close_brace = match_braces(text, open_brace)
    if close_brace == -1:
        return []
    body = text[open_brace + 1:close_brace]

    methods: list[str] = []
    # A method: `$name(args): type;`
    for found in re.finditer(r"^\s*\$(\w+)\s*\(", body, re.M):
        if found.group(1) not in methods:
            methods.append(found.group(1))
    # An event: `readonly $name: vscode.Event<...>;`
    for found in re.finditer(r"^\s*readonly\s+\$(\w+)\s*:", body, re.M):
        if found.group(1) not in methods:
            methods.append(found.group(1))
    return methods


def registered_handlers() -> set[str]:
    """Returns the handler keys that Vrutti registers."""
    text = PROTOCOL_CPP.read_text(encoding="utf-8", errors="replace")
    return set(HANDLER_RE.findall(text))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit with an error when a method is missing")
    args = parser.parse_args()

    protocol = None
    for candidate in UPSTREAM_PROTOCOL_CANDIDATES:
        if candidate.exists():
            protocol = candidate
            break
    if protocol is None:
        print("[audit] the upstream protocol definition was not found", file=sys.stderr)
        return 2

    text = protocol.read_text(encoding="utf-8", errors="replace")
    shapes = main_context_shapes(text)
    if not shapes:
        print("[audit] the MainContext registry was not found", file=sys.stderr)
        return 2

    handled = registered_handlers()

    missing: list[str] = []
    total = 0
    for key in sorted(shapes):
        for method in interface_methods(text, shapes[key]):
            total += 1
            if f"{key}.${method}" not in handled:
                missing.append(f"{key}.${method}")

    print(f"[audit] upstream shapes: {len(shapes)}")
    print(f"[audit] upstream methods: {total}")
    print(f"[audit] handled by Vrutti: {total - len(missing)}")
    print(f"[audit] missing: {len(missing)}")

    if missing:
        by_shape: dict[str, list[str]] = {}
        for key in missing:
            shape, _, method = key.partition(".")
            by_shape.setdefault(shape, []).append(method)
        print()
        for shape in sorted(by_shape):
            print(f"  {shape} ({len(by_shape[shape])}):")
            for method in by_shape[shape]:
                print(f"    ${method}")

    if args.check and missing:
        print(f"\n[audit] {len(missing)} method(s) have no handler", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
