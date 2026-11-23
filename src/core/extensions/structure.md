# extensions (src/core/extensions/)

## Purpose
The extensions part of Vrutti.

## Files
- **ExtensionService.cpp**: Implementation of the ExtensionService component.
- **ExtensionService.h**: Header for the ExtensionService component.

## Public Methods
- **std::filesystem::path ExtensionService::userExtensionsDirectory()**
- **std::filesystem::path ExtensionService::builtinExtensionsDirectory()**
- **std::filesystem::path ExtensionService::metadataFile()**
- **bool ExtensionService::installVsix(const std::string&, ExtensionDescription&, std::string&, std::string&)**
- **bool ExtensionService::uninstall(const std::string&, std::string&, std::string&)**
- **bool ExtensionService::setEnabled(const std::string&, bool, std::string&, std::string&)**
- **std::vector<ExtensionDescription> ExtensionService::listAll()**
- **std::vector<ExtensionDescription> ExtensionService::listEnabled()**
- **std::shared_ptr<vrutti::core::utils::JsonNode> ExtensionService::descriptionsJson(const std::vector<ExtensionDescription>&)**
- **std::shared_ptr<vrutti::core::utils::JsonNode> ExtensionService::userInterfaceJson(const std::vector<ExtensionDescription>&)**
- **bool ExtensionService::readManifest(const std::filesystem::path&, ExtensionDescription&, std::string&)**
- **bool ExtensionService::isCompatible(const std::string&, std::string&)**
- **const char* ExtensionService::hostVersion()**
- **std::string ExtensionService::stableUuid(const std::string&)**
