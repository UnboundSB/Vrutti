#!/usr/bin/env python3
"""Keeps a `structure.md` in every first-party folder of the repository.

Each generated file describes the folder in the same three blocks:

  * **Files**       -- every file in the folder with a one-line description.
  * **Public Methods** -- only the API the folder exposes to the rest of the
    application. Internal helpers stay out.
  * **Child Directories** -- the folders directly inside it, one line each.

Descriptions come from the file itself whenever possible: the first doc
comment is used, otherwise a short template based on the file extension.
Folder purposes come from a small curated table, then from the folder's
`TODO.md`, and only then from a generated sentence.

Usage:

    python scripts/generate_structure_docs.py            # write the files
    python scripts/generate_structure_docs.py --check    # verify only
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

# Folders that are not part of the application source: vendored headers and
# libraries, downloaded tools, third-party test extensions and build output.
# Documenting them would mean editing code that belongs to somebody else.
SKIP_DIR_NAMES = {
    ".git",
    ".tmp-storage",
    "Vrutti_Release",
    "__pycache__",
    "build",
    "build-linux",
    "build_ucrt64",
    "dist",
    "dist_new",
    "headers",
    "installer",
    "node_modules",
    "out",
    "ripgrep",
    "test_exts",
    "vendor",
}

# Never documented, whatever they contain.
SKIP_DIR_PREFIXES = (".git", "build", "node_modules", "Vrutti_Release")

# Paths that are skipped even though their name would not be.
SKIP_DIR_PATHS = {
    "src/ext/bridge/lib",
}

# Curated folder purposes. Everything else falls back to the folder's own
# TODO.md or to a generated sentence.
FOLDER_PURPOSES = {
    ".": "Root of the Vrutti IDE workspace: build configuration, scripts and documentation.",
    ".agents": "Instructions for the coding agents that work on this repository.",
    "bin": "Executables that ship with the application and are used at runtime.",
    "docs": "Project documentation: architecture notes, compatibility notes and design records.",
    "installer": "Installer sources. The setup is produced with Inno Setup 6.",
    "logos": "Brand assets: the logos and the icons of the application.",
    "scripts": "Build, generation and test scripts used by the development workflow.",
    "scripts/lib": "Helpers shared by the scripts: the test harness, init data and tracing preload.",
    "src": "The application source: the native core, the extension host and the user interface.",
    "src/app": "The application entry point: it wires the core services together and starts the window.",
    "src/core": "The native core: configuration, plugins, IPC, storage and the other services.",
    "src/core/commands": "Command registry. Named actions such as save or undo that the UI, keybindings and extensions can trigger.",
    "src/core/concurrency": "Concurrency helpers: the thread pool and the async primitives used by the core.",
    "src/core/config": "Configuration handling: the settings model and the merge rules.",
    "src/core/configuration": "Configuration services: reading, merging and writing user, workspace and policy settings.",
    "src/core/di": "Dependency injection: the container that builds services and wires them together.",
    "src/core/editor": "Editor core: the text buffer, the piece table and the document model.",
    "src/core/events": "Event bus: the in-process publish and subscribe mechanism.",
    "src/core/fs": "File system services: watching, reading and writing files.",
    "src/core/ipc": "Inter-process communication: the pipe transport and the protocol that connects the core to the extension host.",
    "src/core/memory": "Memory helpers: allocators and the shared buffer utilities.",
    "src/core/notifications": "Notification service: the progress and message queue shown in the UI.",
    "src/core/plugins": "The plugin system: the native plugin interface and the loader that loads plugin libraries.",
    "src/core/secrets": "Secret storage: encrypted values kept out of the plain settings files.",
    "src/core/storage": "Storage service: the on-disk database that holds workspace and editor state.",
    "src/core/telemetry": "Telemetry service: optional, opt-in usage reporting.",
    "src/core/theme": "Theme service: discovery, resolution and translation of colour and icon themes.",
    "src/core/updates": "Update service: checks for and applies new versions of the application.",
    "src/core/utils": "Shared helpers: the JSON parser and serializer, path resolution, string pools and the error code registry.",
    "src/core/webview": "Webview service: the panel that hosts extension webviews.",
    "src/ext": "The extension side: the launcher, the extension host entry point, the API surface and the bundled themes.",
    "src/ext/api": "The public extension API. The shape of the `vscode` namespace that extensions are written against.",
    "src/ext/bin": "The runtime binaries shipped with the application, including the Node.js executable.",
    "src/ext/bridge": "The bridge between the native core and the extension host: the protocol, the client and the server sides.",
    "src/ext/builtin-themes": "The themes that ship with the application: the syntax highlighting and the user interface colours.",
    "src/ext/exthost": "The extension host entry point. Vrutti's own launcher for the shared extension host implementation.",
    "src/ext/legacy": "The previous standalone extension host, kept for reference only. It is not part of the running pipeline.",
    "src/ext/protocols": "Protocol definitions shared between the core and the extension host.",
    "src/plugins": "Feature plugins that are not part of the core, such as the search implementation.",
    "src/plugins/search": "The search plugin: indexing and querying the files of a workspace.",
    "src/tests": "Test suites that ship with the source tree.",
    "src/tests/integration": "Integration tests: they exercise the services through their public interfaces.",
    "src/ui": "The user interface layer: the native window and the web renderer.",
    "src/ui/compositor": "The native compositor that owns the window and the webview.",
    "src/ui/frontend": "The web renderer: a Lit application that draws the workbench.",
    "src/ui/frontend/public": "Static assets served to the renderer.",
    "src/ui/frontend/src": "The renderer sources: the workbench components, the services and the entry point.",
    "src/ui/frontend/src/components": "The renderer components.",
    "src/ui/frontend/src/components/layout": "Layout components: the panels that arrange the workbench.",
    "src/ui/frontend/src/components/workbench": "Workbench components: the editor area, the explorer and the side panels.",
    "src/ui/frontend/src/services": "The renderer services: the bridge to the core, the theme applier and the workspace model.",
}

# -----------------------------------------------------------------------------
# Text helpers
# -----------------------------------------------------------------------------

COMMENT_BLOCK_RE = re.compile(r"/\*\*(.*?)\*/", re.S)
LINE_COMMENT_RE = re.compile(r"^\s*//\s?(.*)$")
HASH_COMMENT_RE = re.compile(r"^\s*#\s?(.*)$")
PREPROCESSOR_RE = re.compile(r"^\s*#\s*(include|pragma|ifndef|ifdef|if|define|endif|else|elif|undef|error|line)")


def first_sentence(text: str, limit: int = 110) -> str:
    """Turns a block of comment text into a single short sentence."""
    text = text.strip()
    if text.startswith("/*"):
        text = text[2:]
    if text.endswith("*/"):
        text = text[:-2]
    lines: list[str] = []
    for raw in text.splitlines():
        line = raw.strip().lstrip("*").strip()
        if not line:
            if lines:
                break
            continue
        if re.fullmatch(r"[-=]{3,}", line):
            continue
        if not lines and line.isupper() and len(line) > 8:
            # A heading such as "WHY VRUTTI NEEDS ITS OWN ENTRY POINT".
            continue
        lines.append(line)
    if not lines:
        return ""
    text = " ".join(lines)
    text = re.sub(r"[-=]{3,}$", "", text).strip()
    text = re.sub(r"\s+", " ", text)
    if len(text) > limit:
        cut = text[:limit].rsplit(" ", 1)[0]
        text = cut + "..."
    return text


LICENCE_MARKERS = (
    "copyright",
    "spdx-license-identifier",
    "all rights reserved",
    "licensed under",
    "permission is hereby granted",
    "redistribution and use in source and binary forms",
)

# Notices that only say "this file is generated" are not a description.
GENERATED_MARKERS = (
    "do not edit",
    "generated file",
    "auto-generated",
)


def _is_licence(text: str) -> bool:
    lowered = text.lower()
    return any(marker in lowered for marker in LICENCE_MARKERS)


def leading_comment_blocks(text: str, limit: int = 8000) -> list[str]:
    """Returns the comment blocks that appear before the first line of code."""
    head = text[:limit]
    blocks: list[str] = []
    lines = head.splitlines()
    index = 0
    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        if not stripped:
            index += 1
            continue
        if stripped.startswith("/*"):
            collected = [stripped]
            while "*/" not in collected[-1] and index + 1 < len(lines):
                index += 1
                collected.append(lines[index].strip())
            blocks.append("\n".join(collected))
            index += 1
            continue
        if stripped.startswith("//"):
            collected = []
            while index < len(lines) and lines[index].strip().startswith("//"):
                collected.append(lines[index].strip().lstrip("/").strip())
                index += 1
            blocks.append("\n".join(collected))
            continue
        if stripped.startswith("#!") or PREPROCESSOR_RE.match(line):
            index += 1
            continue
        break
    return blocks


def doc_comment_of(path: pathlib.Path) -> str:
    """Returns the leading doc comment of a file, if it has one."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""

    hash_comments = path.suffix.lower() in (".py", ".sh", ".yml", ".yaml", ".toml")
    for block in leading_comment_blocks(text):
        if hash_comments and block.lstrip().startswith("#"):
            body = "\n".join(line.lstrip("#").strip() for line in block.splitlines())
        else:
            body = block
        if _is_licence(body):
            continue
        inner = COMMENT_BLOCK_RE.search(body)
        if inner:
            body = inner.group(1)
        lowered = body.lower()
        if any(marker in lowered for marker in GENERATED_MARKERS):
            continue
        description = first_sentence(body)
        if description:
            return description
    return ""


def subject_of(path: pathlib.Path) -> str:
    """A readable name for a file, used in fallback descriptions."""
    name = path.name
    if name in ("LICENSE", "Dockerfile", "Makefile", "CMakeLists.txt"):
        return name
    stem = path.stem
    stem = re.sub(r"[._\-]+", " ", stem).strip()
    return stem or name


def primary_export(path: pathlib.Path) -> str:
    """Returns the name of the exported class of a component, if there is one."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    for match in TS_EXPORT_RE.finditer(text):
        if match.group("kind") == "class":
            return match.group("name")
    return ""


def file_description(path: pathlib.Path) -> str:
    doc = doc_comment_of(path)
    if doc:
        return doc

    name = path.name
    subject = subject_of(path)
    suffix = path.suffix.lower()

    known = {
        ".h": f"Header for the {subject} component.",
        ".hpp": f"Header for the {subject} component.",
        ".hh": f"Header for the {subject} component.",
        ".cpp": f"Implementation of the {subject} component.",
        ".cc": f"Implementation of the {subject} component.",
        ".c": f"Implementation of the {subject} component.",
        ".ts": f"TypeScript module: {subject}.",
        ".tsx": f"TypeScript module: {subject}.",
        ".js": f"JavaScript module: {subject}.",
        ".mjs": f"JavaScript module: {subject}.",
        ".cjs": f"JavaScript module: {subject}.",
        ".py": f"Python script: {subject}.",
        ".ps1": f"PowerShell script: {subject}.",
        ".sh": f"Shell script: {subject}.",
        ".css": f"Stylesheet for {subject}.",
        ".scss": f"Stylesheet for {subject}.",
        ".html": f"HTML page: {subject}.",
        ".json": f"Package data for {subject}.",
        ".jsonc": f"Package data for {subject}.",
        ".md": f"Documentation for {subject}.",
        ".txt": f"Text data for {subject}.",
        ".yml": f"YAML configuration for {subject}.",
        ".yaml": f"YAML configuration for {subject}.",
        ".svg": f"Vector asset: {subject}.",
        ".png": f"Image asset: {subject}.",
        ".jpg": f"Image asset: {subject}.",
        ".ico": f"Icon asset: {subject}.",
        ".wasm": f"WebAssembly module: {subject}.",
        ".node": f"Native add-on: {subject}.",
        ".exe": f"Executable: {subject}.",
        ".dll": f"Plugin library: {subject}.",
        ".so": f"Plugin library: {subject}.",
        ".cmake": f"CMake module: {subject}.",
        ".iss": f"Inno Setup script: {subject}.",
        ".ttf": f"Font asset: {subject}.",
        ".woff": f"Font asset: {subject}.",
        ".woff2": f"Font asset: {subject}.",
    }
    if name == "LICENSE":
        return "The license the project is published under."
    if name == "Dockerfile":
        return "Container image definition for the renderer."
    if name == "Makefile":
        return "Convenience targets for the build."
    if name == "CMakeLists.txt":
        return "The CMake build definition."
    if name == "TODO.md":
        return "Open questions and planned work for this folder."
    if name == ".gitignore":
        return "The ignore rules of the repository."

    if suffix in (".ts", ".tsx", ".js", ".mjs", ".cjs"):
        export = primary_export(path)
        if export:
            return f"The `{export}` module."
    return known.get(suffix, f"The {subject} file.")


def read_purpose(path: pathlib.Path) -> str:
    """Reads the Purpose section of a markdown file."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    match = re.search(r"^##\s+Purpose\s*$", text, re.M)
    if not match:
        return ""
    rest = text[match.end():]
    rest = re.split(r"^##\s+", rest, maxsplit=1, flags=re.M)[0]
    for line in rest.splitlines():
        line = line.strip()
        if line and not line.startswith("**") and not line.startswith("|"):
            return first_sentence(line, 160)
    return ""


# -----------------------------------------------------------------------------
# C++ public API extraction
# -----------------------------------------------------------------------------

CPP_MODIFIERS = {
    "static", "virtual", "explicit", "inline", "constexpr", "friend", "extern",
    "consteval", "constinit",
}

CPP_CONTROL_WORDS = {
    "if", "for", "while", "switch", "return", "else", "catch", "do", "sizeof",
    "delete", "new", "throw", "case", "typedef", "using", "namespace", "class",
    "struct", "enum", "union", "template", "operator", "public", "private",
    "protected", "default", "delete",
}


def strip_cpp_comments(text: str) -> list[str]:
    """Removes comments and preprocessor directives from C++ source."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    lines = []
    for line in text.splitlines():
        if line.lstrip().startswith("#"):
            # A preprocessor directive is not part of the API.
            lines.append("")
            continue
        # Keep string literals intact: only cut at a `//` that is not inside
        # a string.
        result = []
        in_string = False
        index = 0
        while index < len(line):
            char = line[index]
            if char == '"' and (index == 0 or line[index - 1] != "\\"):
                in_string = not in_string
            if not in_string and char == "/" and index + 1 < len(line) and line[index + 1] == "/":
                break
            result.append(char)
            index += 1
        lines.append("".join(result))
    return lines


def _split_arguments(args: str) -> list[str]:
    """Splits an argument list on the top level commas."""
    parts: list[str] = []
    depth = 0
    current: list[str] = []
    for char in args:
        if char in "([<":
            depth += 1
        elif char in ")]>":
            depth -= 1
        if char == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
            continue
        current.append(char)
    tail = "".join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def _argument_types(args: str) -> str:
    """Keeps only the type of each argument, dropping the name."""
    result: list[str] = []
    for part in _split_arguments(args):
        part = part.split("=")[0].strip()
        if not part:
            continue
        # `const std::string& name` -> `const std::string&`
        match = re.match(r"^(.*?)(?:\s+[A-Za-z_]\w*)?$", part)
        text = match.group(1).strip() if match else part
        if not text:
            text = part
        result.append(re.sub(r"\s+", " ", text))
    return ", ".join(result)


def _logical_lines(lines: list[str]) -> list[tuple[int, str]]:
    """Joins the lines of a declaration so that each entry is one statement."""
    result: list[tuple[int, str]] = []
    buffer = ""
    start = 0
    depth = 0
    for number, line in enumerate(lines):
        stripped = line.strip()
        if not buffer and stripped:
            start = number
        buffer += " " + stripped
        depth += line.count("(") - line.count(")")
        depth += line.count("[") - line.count("]")
        if depth <= 0 and (buffer.rstrip().endswith(";")
                           or buffer.rstrip().endswith("{")
                           or buffer.rstrip().endswith("}")):
            result.append((start, re.sub(r"\s+", " ", buffer.strip())))
            buffer = ""
            depth = 0
    if buffer.strip():
        result.append((start, re.sub(r"\s+", " ", buffer.strip())))
    return result


def _declaration_name(statement: str) -> tuple[str, str] | None:
    """Returns (name, return type) of a function declaration."""
    text = statement.rstrip()
    if not text.endswith(";"):
        return None
    text = text[:-1].strip()
    if not text or text.endswith("}"):
        return None
    # The name sits in front of the first top level parenthesis.
    depth = 0
    open_index = -1
    for index, char in enumerate(text):
        if char == "(":
            if depth == 0:
                open_index = index
                break
            depth += 1
        elif char == ")" and depth:
            depth -= 1
    if open_index == -1:
        return None

    head = text[:open_index].strip()
    if "=" in head:
        # An assignment, not a declaration.
        return None
    match = re.search(r"([A-Za-z_~][\w:]*)\s*$", head)
    if not match:
        return None
    name = match.group(1).split("::")[-1]
    if name in CPP_CONTROL_WORDS:
        return None

    # The argument list ends at the matching closing parenthesis.
    depth = 0
    close_index = -1
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                close_index = index
                break
    if close_index == -1:
        return None

    # Anything after the argument list must be a type qualifier only.
    tail = text[close_index + 1:].strip()
    if tail and not re.fullmatch(
            r"(?:const|noexcept|override|final|\s|&|&&|=\s*0|=\s*default|"
            r"=\s*delete|\{\s*\}|\s)*", tail):
        return None

    return_type = head[:match.start()].strip()
    tokens = [token for token in return_type.split()
              if token not in CPP_MODIFIERS]
    return_type = " ".join(tokens).strip()
    # `= 0` and `= default` belong to the declaration, not to the type.
    return_type = return_type.split("=")[0].strip()
    if return_type.endswith("*") or return_type.endswith("&"):
        pass
    return name, return_type


def cpp_public_methods(path: pathlib.Path) -> list[str]:
    """Returns the public API declared by a C++ header."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    lines = strip_cpp_comments(text)
    results: list[str] = []

    # The scopes we are in. A frame is (kind, name, access); kind is "class"
    # for a class or struct, "namespace" for a namespace and "block" for a
    # function body.
    stack: list[list] = []

    for _, raw in _logical_lines(lines):
        statement = raw

        frame = stack[-1] if stack else None

        # An access specifier, possibly followed by the member it applies to.
        access_match = re.match(r"^(public|private|protected)\s*:\s*(.*)$", statement)
        if access_match:
            if frame and frame[0] == "class":
                frame[2] = access_match.group(1)
            statement = access_match.group(2).strip()
            if not statement:
                continue

        opens = statement.endswith("{")
        closes = statement.endswith("}") or statement.endswith("};")

        class_match = re.match(
            r"^(?:template\s*<[^>]*>\s*)?(class|struct|union)\s+([A-Za-z_]\w*)", statement)
        if class_match and opens:
            kind = class_match.group(1)
            stack.append(["class", class_match.group(2),
                          "public" if kind != "class" else "private"])
            continue

        if re.match(r"^namespace\b", statement) and opens:
            name_match = re.search(r"namespace\s+([A-Za-z_:]*)\s*$", statement)
            stack.append(["namespace", name_match.group(1) if name_match else "", "public"])
            continue

        # A definition with a body: `void f() { ... }`. The declaration part
        # is still part of the public API; the body is not.
        inline = False
        declaration = statement
        if "{" in statement:
            declaration = statement[:statement.index("{")].strip() + ";"
            inline = True

        def record(text: str) -> None:
            found = _declaration_name(text)
            if not found:
                return
            name, return_type = found
            if name.startswith("m_") or name.startswith("_"):
                return
            args = _argument_types(_args_of(text))
            owner = frame[1] if frame and frame[0] == "class" else ""
            signature = f"{owner}::{name}({args})" if owner else f"{name}({args})"
            if return_type and return_type not in ("void", "auto", ""):
                signature = f"{return_type} {signature}"
            results.append(f"**{signature}**")

        if frame is None or frame[0] == "namespace":
            # A free function at namespace scope.
            record(declaration)
            if opens:
                stack.append(["block", "", "public"])
            elif closes and stack:
                stack.pop()
            continue

        if frame[0] != "class":
            # Inside a function body: not a declaration.
            if (opens or closes) and stack:
                stack.pop()
            continue

        if closes and not inline:
            if stack:
                stack.pop()
            continue

        if frame[2] == "public":
            record(declaration)

        if inline:
            # Only push a scope when the body continues on the next lines.
            if opens:
                stack.append(["block", "", "public"])
            continue

    seen: set[str] = set()
    unique: list[str] = []
    for item in results:
        if item not in seen:
            seen.add(item)
            unique.append(item)
    return unique


def _args_of(statement: str) -> str:
    """Returns the argument list of a statement."""
    depth = 0
    start = -1
    for index, char in enumerate(statement):
        if char == "(":
            if depth == 0:
                start = index
                break
            depth += 1
        elif char == ")" and depth:
            depth -= 1
    if start == -1:
        return ""
    depth = 0
    for index in range(start, len(statement)):
        char = statement[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return statement[start + 1:index]
    return ""


# -----------------------------------------------------------------------------
# TypeScript and JavaScript public API extraction
# -----------------------------------------------------------------------------

TS_EXPORT_RE = re.compile(
    r"^\s*export\s+(?:default\s+)?(?:async\s+)?"
    r"(?P<kind>function|class|const|let|var|interface|type|enum)\s+"
    r"(?P<name>[A-Za-z_$][\w$]*)",
    re.M,
)
TS_CLASS_METHOD_RE = re.compile(
    r"^(?P<indent>\s+)(?P<name>[A-Za-z_$][\w$]*)\s*\((?P<args>[^)]*)\)\s*[:{]"
)
TS_EXPORT_LIST_RE = re.compile(r"^\s*export\s*\{(?P<names>[^}]*)\}")
TS_PRIVATE_RE = re.compile(r"^\s*(?:private|protected)\b")
TS_SKIP_METHODS = {
    "constructor", "if", "for", "while", "switch", "catch", "return", "function",
}


def ts_public_methods(path: pathlib.Path) -> list[str]:
    """Returns the exported API of a TypeScript or JavaScript module."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    results: list[str] = []
    lines = text.splitlines()

    class_name = ""
    class_body: list[str] = []
    in_class = False
    brace_depth = 0

    for line in lines:
        if not in_class:
            match = TS_EXPORT_RE.match(line)
            if match and match.group("kind") == "class":
                class_name = match.group("name")
                in_class = True
                brace_depth = line.count("{") - line.count("}")
                class_body = []
                continue
            if match:
                name = match.group("name")
                if match.group("kind") in ("function", "const", "let", "var"):
                    results.append(f"**{name}()**")
                else:
                    results.append(f"**{name}** (exported type)")
                continue
            exported = TS_EXPORT_LIST_RE.match(line)
            if exported:
                for part in exported.group("names").split(","):
                    part = part.strip().split(" as ")[-1].strip()
                    if part:
                        results.append(f"**{part}** (re-export)")
                continue
            continue

        class_body.append(line)
        brace_depth += line.count("{") - line.count("}")
        if brace_depth <= 0:
            depth = 0
            for body_line in class_body:
                depth += body_line.count("{") - body_line.count("}")
                if depth != 1:
                    continue
                if TS_PRIVATE_RE.match(body_line):
                    continue
                method = TS_CLASS_METHOD_RE.match(body_line)
                if not method:
                    continue
                name = method.group("name")
                if name in TS_SKIP_METHODS or name.startswith("_"):
                    continue
                args = " ".join(method.group("args").split())
                results.append(f"**{class_name}.{name}({args})**")
            in_class = False
            class_name = ""
            class_body = []

    # `module.exports = { a, b }` is the CommonJS form of the same thing.
    for match in re.finditer(r"module\.exports\s*=\s*\{(?P<body>[^}]*)\}", text):
        for part in match.group("body").split(","):
            part = part.strip().split(":")[0].strip()
            if re.fullmatch(r"[A-Za-z_$][\w$]*", part):
                results.append(f"**{part}()** (exported)")

    seen: set[str] = set()
    unique: list[str] = []
    for item in results:
        if item not in seen:
            seen.add(item)
            unique.append(item)
    return unique


# -----------------------------------------------------------------------------
# Python public API extraction
# -----------------------------------------------------------------------------

PY_DEF_RE = re.compile(r"^(?:async\s+)?def\s+(?P<name>[A-Za-z_]\w*)\s*\(")
PY_CLASS_RE = re.compile(r"^class\s+(?P<name>[A-Za-z_]\w*)")


def py_public_methods(path: pathlib.Path) -> list[str]:
    """Returns the public functions and classes of a Python module."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    results: list[str] = []
    for line in text.splitlines():
        if line.startswith("def ") or line.startswith("async def "):
            match = PY_DEF_RE.match(line)
            if match and not match.group("name").startswith("_"):
                results.append(f"**{match.group('name')}()**")
        elif line.startswith("class "):
            match = PY_CLASS_RE.match(line)
            if match and not match.group("name").startswith("_"):
                results.append(f"**{match.group('name')}** (class)")
    return results


def public_methods_of(path: pathlib.Path) -> list[str]:
    suffix = path.suffix.lower()
    if suffix in (".h", ".hpp", ".hh"):
        return cpp_public_methods(path)
    if suffix in (".ts", ".tsx", ".js", ".mjs", ".cjs"):
        return ts_public_methods(path)
    if suffix == ".py":
        return py_public_methods(path)
    return []


# -----------------------------------------------------------------------------
# Folder discovery and rendering
# -----------------------------------------------------------------------------

def is_skipped(path: pathlib.Path) -> bool:
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel in SKIP_DIR_PATHS:
        return True
    for part in pathlib.PurePosixPath(rel).parts:
        if part in SKIP_DIR_NAMES:
            return True
        if any(part.startswith(prefix) for prefix in SKIP_DIR_PREFIXES):
            return True
    return False


def discover_dirs() -> list[pathlib.Path]:
    result = [REPO_ROOT]
    for path in sorted(REPO_ROOT.rglob("*")):
        if path.is_dir() and not is_skipped(path):
            result.append(path)
    return result


def relative(path: pathlib.Path) -> str:
    rel = path.relative_to(REPO_ROOT).as_posix()
    return "." if rel == "." else rel


def folder_purpose(path: pathlib.Path) -> str:
    rel = relative(path)
    if rel in FOLDER_PURPOSES:
        return FOLDER_PURPOSES[rel]
    todo = path / "TODO.md"
    if todo.exists():
        purpose = read_purpose(todo)
        if purpose:
            return purpose
    leaf = path.name
    return f"The {leaf} part of Vrutti."


def child_description(child: pathlib.Path) -> str:
    # The curated table comes first: reading the purpose from the generated
    # file of the child would make the result depend on the order in which
    # the folders are written.
    rel = relative(child)
    if rel in FOLDER_PURPOSES:
        return FOLDER_PURPOSES[rel]
    todo = child / "TODO.md"
    if todo.exists():
        purpose = read_purpose(todo)
        if purpose:
            return purpose
    leaf = child.name
    return f"The {leaf} module."


def render(path: pathlib.Path) -> str:
    rel = relative(path)
    leaf = path.name if rel != "." else "vrutti_ide"
    title = f"# {leaf} ({rel}/)" if rel != "." else f"# {leaf} (repository root)"

    out: list[str] = [title, "", "## Purpose", folder_purpose(path), ""]

    files = sorted(
        (p for p in path.iterdir() if p.is_file() and p.name != "structure.md"),
        key=lambda p: p.name.lower(),
    )
    if files:
        out.append("## Files")
        for item in files:
            out.append(f"- **{item.name}**: {file_description(item)}")
        out.append("")

    methods: list[str] = []
    for item in files:
        if item.suffix.lower() in (".md", ".json", ".txt", ".yml", ".yaml", ".css"):
            continue
        for method in public_methods_of(item):
            methods.append(f"- {method}")
    if methods:
        out.append("## Public Methods")
        out.extend(methods)
        out.append("")

    children = sorted((p for p in path.iterdir() if p.is_dir() and not is_skipped(p)),
                      key=lambda p: p.name.lower())
    if children:
        out.append("## Child Directories")
        for child in children:
            out.append(f"- **{child.name}/**: {child_description(child)}")
        out.append("")

    return "\n".join(out).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the generated files are up to date")
    args = parser.parse_args()

    dirs = discover_dirs()
    stale: list[str] = []
    for path in dirs:
        content = render(path)
        target = path / "structure.md"
        current = target.read_text(encoding="utf-8") if target.exists() else ""
        if current == content:
            continue
        stale.append(relative(path))
        if args.check:
            print(f"[structure] {relative(path)}/structure.md is out of date",
                  file=sys.stderr)
            continue
        target.write_text(content, encoding="utf-8", newline="\n")
        print(f"[structure] wrote {relative(path)}/structure.md")

    if args.check and stale:
        print(f"[structure] {len(stale)} file(s) out of date", file=sys.stderr)
        return 1
    print(f"[structure] {len(dirs)} folders, {len(stale)} updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
