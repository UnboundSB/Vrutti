# scripts (scripts/)

## Purpose
Build, generation and test scripts used by the development workflow.

## Files
- **audit_exthost_handlers.py**: Python script: audit exthost handlers.
- **build_exthost.js**: Builds the Vrutti Extension Host bundle.
- **generate_error_codes.py**: Python script: generate error codes.
- **generate_exthost_fallbacks.py**: Python script: generate exthost fallbacks.
- **generate_proxy_ids.py**: Python script: generate proxy ids.
- **generate_structure_docs.py**: Python script: generate structure docs.
- **test_exthost_activation.js**: End-to-end activation test for the Vrutti <-> VS Code Extension Host link.
- **test_exthost_handshake.js**: Integration test for the Vrutti <-> VS Code Extension Host handshake.
- **test_json.cpp**: Round-trip tests for the JSON layer.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **match_braces()**
- **main_context_shapes()**
- **interface_methods()**
- **registered_handlers()**
- **main()**
- **parse_codes()**
- **parse_cpp_table()**
- **subsystem_of()**
- **render_js()**
- **render_doc()**
- **write_or_check()**
- **main()**
- **match_braces()**
- **main_context_shapes()**
- **split_top_level()**
- **interface_members()**
- **classify()**
- **main()**
- **parse_proxy_table()**
- **render()**
- **main()**
- **first_sentence()**
- **leading_comment_blocks()**
- **doc_comment_of()**
- **subject_of()**
- **primary_export()**
- **file_description()**
- **read_purpose()**
- **strip_cpp_comments()**
- **cpp_public_methods()**
- **ts_public_methods()**
- **py_public_methods()**
- **public_methods_of()**
- **is_skipped()**
- **discover_dirs()**
- **relative()**
- **folder_purpose()**
- **child_description()**
- **render()**
- **main()**

## Child Directories
- **lib/**: Helpers shared by the scripts: the test harness, init data and tracing preload.
