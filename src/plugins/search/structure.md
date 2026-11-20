# search (src/plugins/search/)

## Purpose
The search plugin: indexing and querying the files of a workspace.

## Files
- **SearchPlugin.cpp**: Implementation of the SearchPlugin component.
- **SearchPlugin.h**: Header for the SearchPlugin component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **SearchPlugin::SearchPlugin()**
- **SearchPlugin::~SearchPlugin()**
- **bool SearchPlugin::initialize()**
- **SearchPlugin::shutdown()**
- **const char* SearchPlugin::getName()**
- **std::string SearchPlugin::executeCommand(const std::string&, const std::string&)**
- **SearchResponse SearchPlugin::performSearch(const std::string&, const std::string&, const SearchOptions&)**
- **std::vector<std::string> SearchPlugin::findFiles(const std::string&)**
