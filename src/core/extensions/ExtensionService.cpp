#include "ExtensionService.h"
#include "../utils/Json.h"
#include "../utils/ZipArchive.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::core::utils::JsonSerializer;
using vrutti::core::utils::ZipArchive;
using vrutti::core::utils::ZipEntry;

namespace vrutti::core::extensions {

namespace {

    // The name of the folder of the extension inside of a `.vsix` file.
    constexpr const char* VSIX_EXTENSION_PREFIX = "extension/";

    // The name of the file that holds the manifest of an extension.
    constexpr const char* MANIFEST_NAME = "package.json";

    // The version of the host that runs the extensions. An extension that
    // asks for a newer one is not installed.
    constexpr const char* HOST_VERSION_MAJOR = "1";
    constexpr const char* HOST_VERSION_MINOR = "90";
    constexpr const char* HOST_VERSION_PATCH = "0";

    // The manifest of an extension has to name it with a name that can be
    // part of a folder name.
    bool isSafeName(const std::string& value) {
        if (value.empty() || value.size() > 128) return false;
        if (value == "." || value == "..") return false;
        for (unsigned char c : value) {
            if (std::isalnum(c)) continue;
            if (c == '.' || c == '-' || c == '_') continue;
            return false;
        }
        return true;
    }

    // The version of an extension is used in the name of its folder, so it
    // has to be safe as well.
    bool isSafeVersion(const std::string& value) {
        if (value.empty() || value.size() > 64) return false;
        for (unsigned char c : value) {
            if (std::isalnum(c)) continue;
            if (c == '.' || c == '-' || c == '+') continue;
            return false;
        }
        return true;
    }

    std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    // The home folder of the user.
    fs::path homeDirectory() {
#ifdef _WIN32
        const char* profile = std::getenv("USERPROFILE");
        if (profile && *profile) return fs::path(profile);
        const char* drive = std::getenv("HOMEDRIVE");
        const char* path = std::getenv("HOMEPATH");
        if (drive && path) return fs::path(drive) / path;
#else
        const char* home = std::getenv("HOME");
        if (home && *home) return fs::path(home);
#endif
        return fs::current_path();
    }

    std::string stringOf(const std::shared_ptr<JsonNode>& node) {
        if (!node || node->type != JsonNode::Type::String) return "";
        return std::string(node->stringValue.data(), node->stringValue.length());
    }

    // A `file:` URI in the shape of the upstream URI DTO.
    std::shared_ptr<JsonNode> makeFileUri(const fs::path& path) {
        std::string p = path.generic_string();
        if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
            p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        }
        if (p.empty() || p[0] != '/') p = "/" + p;

        auto uri = std::make_shared<JsonNode>(JsonNode::Type::Object);
        auto mid = std::make_shared<JsonNode>(JsonNode::Type::Number);
        mid->numberValue = 1; // MarshalledId.Uri
        uri->objectProperties.emplace_back("$mid", mid);
        uri->objectProperties.emplace_back("scheme", vrutti::core::utils::makeJsonString("file"));
        uri->objectProperties.emplace_back("path", vrutti::core::utils::makeJsonString(p));
        return uri;
    }

    // The version of an extension, as three numbers.
    bool parseVersion(const std::string& text, int& major, int& minor, int& patch) {
        major = minor = patch = 0;
        size_t i = 0;
        int numbers = 0;
        while (i < text.size() && numbers < 3) {
            if (!std::isdigit(static_cast<unsigned char>(text[i]))) break;
            int value = 0;
            while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
                value = value * 10 + (text[i] - '0');
                if (value > 1000000) return false;
                ++i;
            }
            if (numbers == 0) major = value;
            else if (numbers == 1) minor = value;
            else patch = value;
            ++numbers;
            if (i < text.size() && text[i] == '.') ++i;
            else break;
        }
        return numbers > 0;
    }

    // Reads the manifest of an extension out of a parsed JSON tree.
    bool readManifestFromNode(const std::shared_ptr<JsonNode>& manifest,
                              ExtensionDescription& out,
                              std::string& error) {
        if (!manifest || manifest->type != JsonNode::Type::Object) {
            error = "the manifest of the extension is not valid";
            return false;
        }

        const std::string name = stringOf(manifest->get("name"));
        const std::string publisher = stringOf(manifest->get("publisher"));
        if (!isSafeName(name)) {
            error = "the manifest of the extension has no usable name";
            return false;
        }
        if (!isSafeName(publisher)) {
            error = "the manifest of the extension has no usable publisher";
            return false;
        }

        std::string version = stringOf(manifest->get("version"));
        if (version.empty()) version = "0.0.0";
        if (!isSafeVersion(version)) {
            error = "the manifest of the extension has no usable version";
            return false;
        }

        out.name = name;
        out.publisher = publisher;
        out.identifier = publisher + "." + name;
        out.lowerIdentifier = toLower(out.identifier);
        out.uuid = ExtensionService::stableUuid(out.identifier);
        out.version = version;
        out.publisherDisplayName = publisher;
        out.displayName = stringOf(manifest->get("displayName"));
        if (out.displayName.empty()) out.displayName = name;
        out.description = stringOf(manifest->get("description"));
        out.main = stringOf(manifest->get("main"));
        out.browser = stringOf(manifest->get("browser"));
        out.enginesVscode = "^1.90.0";

        if (auto engines = manifest->get("engines");
            engines && engines->type == JsonNode::Type::Object) {
            const std::string asked = stringOf(engines->get("vscode"));
            if (!asked.empty()) out.enginesVscode = asked;
        }

        if (auto events = manifest->get("activationEvents");
            events && events->type == JsonNode::Type::Array) {
            for (const auto& event : events->arrayElements) {
                const std::string text = stringOf(event);
                if (!text.empty()) out.activationEvents.push_back(text);
            }
        }

        if (auto contributes = manifest->get("contributes");
            contributes && contributes->type == JsonNode::Type::Object) {
            out.contributes = contributes;
        }
        return true;
    }

    // A name of a folder that is unique for this run, so that two
    // installations never work in the same place.
    std::string uniqueFolderSuffix() {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        std::ostringstream out;
        out << now << "-" << std::hash<std::string>{}(std::to_string(now));
        return out.str();
    }

} // namespace

// ---------------------------------------------------------------------------
// The folders
// ---------------------------------------------------------------------------

fs::path ExtensionService::userExtensionsDirectory() {
    const char* override = std::getenv("VRUTTI_EXTENSIONS_DIR");
    if (override && *override) return fs::path(override);
    return homeDirectory() / ".vrutti" / "extensions";
}

fs::path ExtensionService::builtinExtensionsDirectory() {
    const char* override = std::getenv("VRUTTI_BUILTIN_EXTENSIONS_DIR");
    if (override && *override) return fs::path(override);
    return fs::current_path() / "extensions";
}

fs::path ExtensionService::metadataFile() {
    return userExtensionsDirectory() / "extensions.json";
}

const char* ExtensionService::hostVersion() {
    return "1.90.0";
}

// ---------------------------------------------------------------------------
// The identity of an extension
// ---------------------------------------------------------------------------

std::string ExtensionService::stableUuid(const std::string& seed) {
    // The same idea as the upstream: the identity of an extension does not
    // change between two runs, so the extension keeps its data.
    uint64_t h1 = 1469598103934665603ULL;
    uint64_t h2 = 1099511628211ULL;
    for (unsigned char c : seed) {
        h1 ^= c; h1 *= 1099511628211ULL;
        h2 ^= c; h2 *= 1469598103934665603ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    out << std::setw(8) << (h1 & 0xFFFFFFFFULL);
    out << std::setw(4) << ((h1 >> 32) & 0xFFFFULL);
    out << std::setw(4) << ((h2 >> 16) & 0xFFFFULL);
    out << std::setw(4) << ((h2 >> 32) & 0xFFFFULL);
    out << std::setw(12) << ((h1 ^ h2) & 0xFFFFFFFFFFFFULL);
    const std::string hex = out.str();
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" +
           hex.substr(16, 4) + "-" + hex.substr(20, 12);
}

// ---------------------------------------------------------------------------
// The manifest of an extension
// ---------------------------------------------------------------------------

bool ExtensionService::readManifest(const fs::path& extensionRoot,
                                    ExtensionDescription& out,
                                    std::string& error) {
    const fs::path manifestPath = extensionRoot / MANIFEST_NAME;
    std::ifstream input(manifestPath, std::ios::binary);
    if (!input.is_open()) {
        error = "the extension has no manifest";
        return false;
    }
    std::stringstream buffer;
    buffer << input.rdbuf();

    auto manifest = JsonParser::parse(buffer.str());
    if (!readManifestFromNode(manifest, out, error)) return false;

    out.extensionLocation = extensionRoot.generic_string();
    return true;
}

// ---------------------------------------------------------------------------
// The version of an extension against the version of the host
// ---------------------------------------------------------------------------

bool ExtensionService::isCompatible(const std::string& enginesVscode,
                                    std::string& error) {
    if (enginesVscode.empty()) return true;

    // The range is read as the *lowest* version that the extension accepts.
    // The ranges that Vrutti cannot read are accepted: an extension that
    // asks for something strange is better refused by the extension host
    // than by a guess in the installer.
    std::string range = enginesVscode;
    const size_t space = range.find(' ');
    if (space != std::string::npos) range = range.substr(0, space);
    if (range.empty()) return true;

    size_t start = 0;
    while (start < range.size() &&
           (range[start] == '^' || range[start] == '~' || range[start] == '>' ||
            range[start] == '=' || range[start] == '<' || range[start] == ' ')) {
        ++start;
    }
    const std::string wanted = range.substr(start);

    int major = 0, minor = 0, patch = 0;
    if (!parseVersion(wanted, major, minor, patch)) return true;

    int hostMajor = 0, hostMinor = 0, hostPatch = 0;
    parseVersion(hostVersion(), hostMajor, hostMinor, hostPatch);

    const bool newer = major > hostMajor ||
        (major == hostMajor && minor > hostMinor) ||
        (major == hostMajor && minor == hostMinor && patch > hostPatch);
    if (newer) {
        error = "the extension asks for version " + wanted +
                " of the host, but this host is version " + hostVersion();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Installing an extension
// ---------------------------------------------------------------------------

bool ExtensionService::installVsix(const std::string& vsixPath,
                                   ExtensionDescription& installed,
                                   std::string& error,
                                   std::string& errorCode) {
    std::error_code ec;

    if (!fs::exists(vsixPath, ec) || fs::is_directory(vsixPath, ec)) {
        error = "the file of the extension does not exist";
        errorCode = "FileNotFound";
        return false;
    }

    ZipArchive archive;
    if (!archive.open(vsixPath, error)) {
        errorCode = "InvalidExtension";
        return false;
    }

    // The manifest has to be in the archive, and it is read *before* anything
    // is written to the disk.
    //
    // A `.vsix` file holds the extension under the folder `extension`. A
    // plain archive of an extension holds it at the root. Both are accepted,
    // and the layout decides which one it is.
    const ZipEntry* manifestEntry = nullptr;
    bool hasExtensionFolder = false;
    for (const auto& entry : archive.entries()) {
        if (entry.name == "extension/package.json") {
            manifestEntry = &entry;
            hasExtensionFolder = true;
            break;
        }
        if (entry.name == "package.json" && !manifestEntry) {
            manifestEntry = &entry;
        }
    }
    if (!manifestEntry) {
        error = "the file is not an extension: it has no manifest";
        errorCode = "InvalidExtension";
        return false;
    }

    std::string manifestText;
    if (!archive.read(*manifestEntry, manifestText, error)) {
        errorCode = "InvalidExtension";
        return false;
    }

    auto manifest = JsonParser::parse(manifestText);
    if (!manifest || manifest->type != JsonNode::Type::Object) {
        error = "the manifest of the extension is not valid";
        errorCode = "InvalidExtension";
        return false;
    }

    ExtensionDescription description;
    std::string manifestError;
    if (!readManifestFromNode(manifest, description, manifestError)) {
        error = manifestError;
        errorCode = "InvalidExtension";
        return false;
    }

    if (!isCompatible(description.enginesVscode, error)) {
        errorCode = "InvalidExtension";
        return false;
    }

    // --- the folder of the extension -----------------------------------------
    const fs::path extensionsDir = userExtensionsDirectory();
    fs::create_directories(extensionsDir, ec);
    if (ec && !fs::exists(extensionsDir)) {
        error = "the folder of the extensions could not be created";
        errorCode = "Unavailable";
        return false;
    }

    const std::string folderName = description.identifier + "-" + description.version;
    const fs::path target = extensionsDir / folderName;

    // The extension is unpacked into a folder of its own first. When the
    // unpacking fails, nothing is half-written: the folder is removed and
    // the old version of the extension stays as it was.
    const fs::path staging = extensionsDir / (".install-" + uniqueFolderSuffix());
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec && !fs::exists(staging)) {
        error = "the folder of the extension could not be created";
        errorCode = "Unavailable";
        return false;
    }

    const bool extracted = hasExtensionFolder
        ? archive.extractAll(staging.string(), VSIX_EXTENSION_PREFIX, error)
        : archive.extractAll(staging.string(), "", error);
    if (!extracted) {
        fs::remove_all(staging, ec);
        errorCode = "InvalidExtension";
        return false;
    }

    // The manifest has to be at the root of the unpacked extension.
    if (!fs::exists(staging / MANIFEST_NAME)) {
        fs::remove_all(staging, ec);
        error = "the file is not an extension: it has no manifest";
        errorCode = "InvalidExtension";
        return false;
    }

    // The entry point of the extension has to be there.
    const std::string entryPoint = description.main.empty() ? description.browser : description.main;
    if (!entryPoint.empty()) {
        const fs::path entryPath = staging / entryPoint;
        std::error_code entryError;
        if (!fs::exists(entryPath, entryError) || fs::is_directory(entryPath, entryError)) {
            fs::remove_all(staging, ec);
            error = "the entry point of the extension is missing: " + entryPoint;
            errorCode = "InvalidExtension";
            return false;
        }
    }

    // --- the folder is put in place ------------------------------------------
    if (fs::exists(target, ec)) {
        fs::remove_all(target, ec);
    }
    fs::rename(staging, target, ec);
    if (ec) {
        fs::remove_all(staging, ec);
        error = "the extension could not be put in place";
        errorCode = "Unavailable";
        return false;
    }

    description.extensionLocation = target.generic_string();
    description.relativeLocation = folderName;
    description.isBuiltin = false;
    description.enabled = true;

    // The older versions of the same extension go away: two versions of one
    // extension in the folder would both be given to the host.
    removeOlderVersions(description.identifier, folderName);

    // --- the metadata ---------------------------------------------------------
    auto installedList = readMetadata();
    bool found = false;
    for (auto& entry : installedList) {
        if (entry.lowerIdentifier == description.lowerIdentifier) {
            entry = description;
            found = true;
            break;
        }
    }
    if (!found) installedList.push_back(description);

    std::string metadataError;
    if (!writeMetadata(installedList, metadataError)) {
        error = metadataError;
        errorCode = "Unavailable";
        return false;
    }

    installed = description;
    return true;
}

// ---------------------------------------------------------------------------
// Removing an extension
// ---------------------------------------------------------------------------

bool ExtensionService::uninstall(const std::string& identifier,
                                 std::string& error,
                                 std::string& errorCode) {
    const std::string wanted = toLower(identifier);
    if (wanted.empty()) {
        error = "the name of the extension is missing";
        errorCode = "InvalidExtension";
        return false;
    }

    // The name of an extension may be given without its publisher. The
    // publisher is then not part of the name, so the extension is looked up
    // by the name of the extension alone.
    const bool onlyName = wanted.find('.') == std::string::npos;

    auto installedList = readMetadata();
    std::vector<ExtensionDescription> remaining;
    bool found = false;
    for (const auto& entry : installedList) {
        const bool matches = entry.lowerIdentifier == wanted ||
            (onlyName && toLower(entry.name) == wanted);
        if (matches) {
            found = true;
            std::error_code ec;
            // The name of the folder has to be a single part of a name. A
            // metadata file that was edited by hand must not be able to
            // point outside of the folder of the extensions.
            const std::string& location = entry.relativeLocation;
            const bool safe = !location.empty() &&
                location.find('/') == std::string::npos &&
                location.find('\\') == std::string::npos &&
                location != "." && location != "..";
            if (safe) {
                fs::remove_all(userExtensionsDirectory() / location, ec);
            }
            continue;
        }
        remaining.push_back(entry);
    }

    if (!found) {
        error = "the extension is not installed";
        errorCode = "FileNotFound";
        return false;
    }

    std::string metadataError;
    if (!writeMetadata(remaining, metadataError)) {
        error = metadataError;
        errorCode = "Unavailable";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Turning an extension on and off
// ---------------------------------------------------------------------------

bool ExtensionService::setEnabled(const std::string& identifier,
                                  bool enabled,
                                  std::string& error,
                                  std::string& errorCode) {
    const std::string wanted = toLower(identifier);
    const bool onlyName = wanted.find('.') == std::string::npos;
    auto installedList = readMetadata();
    bool found = false;
    for (auto& entry : installedList) {
        if (entry.lowerIdentifier == wanted ||
            (onlyName && toLower(entry.name) == wanted)) {
            entry.enabled = enabled;
            found = true;
            break;
        }
    }
    if (!found) {
        error = "the extension is not installed";
        errorCode = "FileNotFound";
        return false;
    }

    std::string metadataError;
    if (!writeMetadata(installedList, metadataError)) {
        error = metadataError;
        errorCode = "Unavailable";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The list of the extensions
// ---------------------------------------------------------------------------

bool ExtensionService::describeFolder(const fs::path& root,
                                      bool isBuiltin,
                                      ExtensionDescription& out) {
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return false;

    // The manifest is at the root of the folder, or in the folder `extension`
    // of the layout of the marketplace.
    fs::path extensionRoot = root;
    if (!fs::exists(extensionRoot / MANIFEST_NAME, ec)) {
        const fs::path nested = root / "extension";
        if (fs::exists(nested / MANIFEST_NAME, ec)) {
            extensionRoot = nested;
        }
    }

    std::string error;
    if (!readManifest(extensionRoot, out, error)) return false;
    out.isBuiltin = isBuiltin;
    out.relativeLocation = root.filename().string();
    return true;
}

std::vector<ExtensionDescription> ExtensionService::listAll() {
    std::vector<ExtensionDescription> result;
    std::error_code ec;

    // The state of the extensions of the user comes from the metadata file:
    // it knows which ones are turned off.
    auto metadata = readMetadata();

    const fs::path builtinDir = builtinExtensionsDirectory();
    if (fs::exists(builtinDir, ec) && fs::is_directory(builtinDir, ec)) {
        for (const auto& entry : fs::directory_iterator(builtinDir, ec)) {
            if (!entry.is_directory()) continue;
            ExtensionDescription description;
            if (!describeFolder(entry.path(), true, description)) continue;
            result.push_back(description);
        }
    }

    const fs::path userDir = userExtensionsDirectory();
    if (fs::exists(userDir, ec) && fs::is_directory(userDir, ec)) {
        for (const auto& entry : fs::directory_iterator(userDir, ec)) {
            if (!entry.is_directory()) continue;

            ExtensionDescription description;
            if (!describeFolder(entry.path(), false, description)) continue;

            // The metadata file has the last word about the state of an
            // extension: it knows that one was turned off.
            for (const auto& known : metadata) {
                if (known.lowerIdentifier == description.lowerIdentifier) {
                    description.enabled = known.enabled;
                    description.relativeLocation = known.relativeLocation.empty()
                        ? description.relativeLocation : known.relativeLocation;
                    break;
                }
            }
            result.push_back(description);
        }
    }

    return result;
}

std::vector<ExtensionDescription> ExtensionService::listEnabled() {
    std::vector<ExtensionDescription> result;
    for (const auto& description : listAll()) {
        if (description.enabled) result.push_back(description);
    }
    return result;
}

// ---------------------------------------------------------------------------
// The metadata file
// ---------------------------------------------------------------------------

std::vector<ExtensionDescription> ExtensionService::readMetadata() {
    std::vector<ExtensionDescription> result;
    std::ifstream input(metadataFile(), std::ios::binary);
    if (!input.is_open()) return result;

    std::stringstream buffer;
    buffer << input.rdbuf();
    auto root = JsonParser::parse(buffer.str());
    if (!root || root->type != JsonNode::Type::Object) return result;

    auto installed = root->get("installed");
    if (!installed || installed->type != JsonNode::Type::Array) return result;

    for (const auto& item : installed->arrayElements) {
        if (!item || item->type != JsonNode::Type::Object) continue;
        ExtensionDescription description;

        if (auto identifier = item->get("identifier");
            identifier && identifier->type == JsonNode::Type::Object) {
            description.identifier = stringOf(identifier->get("value"));
            description.lowerIdentifier = stringOf(identifier->get("_lower"));
        }
        if (description.identifier.empty()) continue;
        if (description.lowerIdentifier.empty()) {
            description.lowerIdentifier = toLower(description.identifier);
        }

        description.uuid = stringOf(item->get("uuid"));
        if (description.uuid.empty()) {
            description.uuid = stableUuid(description.identifier);
        }
        description.version = stringOf(item->get("version"));
        description.relativeLocation = stringOf(item->get("relativeLocation"));
        description.extensionLocation = stringOf(item->get("location"));

        const size_t dot = description.identifier.find('.');
        if (dot != std::string::npos) {
            description.publisher = description.identifier.substr(0, dot);
            description.name = description.identifier.substr(dot + 1);
        }
        description.publisherDisplayName = description.publisher;
        description.displayName = description.name;
        description.enginesVscode = "^1.90.0";

        if (auto enabled = item->get("enabled");
            enabled && enabled->type == JsonNode::Type::Boolean) {
            description.enabled = enabled->boolValue;
        }

        result.push_back(description);
    }
    return result;
}

bool ExtensionService::writeMetadata(const std::vector<ExtensionDescription>& list,
                                     std::string& error) {
    auto installed = std::make_shared<JsonNode>(JsonNode::Type::Array);
    for (const auto& description : list) {
        auto item = std::make_shared<JsonNode>(JsonNode::Type::Object);

        auto identifier = std::make_shared<JsonNode>(JsonNode::Type::Object);
        identifier->objectProperties.emplace_back(
            "value", vrutti::core::utils::makeJsonString(description.identifier));
        identifier->objectProperties.emplace_back(
            "_lower", vrutti::core::utils::makeJsonString(description.lowerIdentifier));
        item->objectProperties.emplace_back("identifier", identifier);

        item->objectProperties.emplace_back(
            "uuid", vrutti::core::utils::makeJsonString(description.uuid));
        item->objectProperties.emplace_back(
            "version", vrutti::core::utils::makeJsonString(description.version));
        item->objectProperties.emplace_back(
            "location", vrutti::core::utils::makeJsonString(description.extensionLocation));
        item->objectProperties.emplace_back(
            "relativeLocation", vrutti::core::utils::makeJsonString(description.relativeLocation));

        auto enabled = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        enabled->boolValue = description.enabled;
        item->objectProperties.emplace_back("enabled", enabled);

        installed->arrayElements.push_back(item);
    }

    auto root = std::make_shared<JsonNode>(JsonNode::Type::Object);
    root->objectProperties.emplace_back("installed", installed);

    std::error_code ec;
    fs::create_directories(userExtensionsDirectory(), ec);

    const fs::path path = metadataFile();
    const fs::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error = "the list of the extensions could not be written";
            return false;
        }
        const std::string text = JsonSerializer::stringify(root, 0, true);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        if (!output) {
            error = "the list of the extensions could not be written";
            return false;
        }
    }
    fs::rename(temporary, path, ec);
    if (ec) {
        // On windows a file cannot be renamed over another one, so the old
        // file goes first.
        std::error_code removeError;
        fs::remove(path, removeError);
        fs::rename(temporary, path, ec);
    }
    if (ec) {
        fs::remove(temporary, ec);
        error = "the list of the extensions could not be written";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The older versions of an extension
// ---------------------------------------------------------------------------

void ExtensionService::removeOlderVersions(const std::string& identifier,
                                           const std::string& keepFolder) {
    std::error_code ec;
    const fs::path extensionsDir = userExtensionsDirectory();
    if (!fs::exists(extensionsDir, ec)) return;

    for (const auto& entry : fs::directory_iterator(extensionsDir, ec)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        if (name == keepFolder) continue;
        // The folder of an extension is "<publisher>.<name>-<version>".
        if (name.size() <= identifier.size() + 1) continue;
        if (name.compare(0, identifier.size(), identifier) != 0) continue;
        if (name[identifier.size()] != '-') continue;
        fs::remove_all(entry.path(), ec);
    }
}

// ---------------------------------------------------------------------------
// The descriptions in the shape of the host
// ---------------------------------------------------------------------------

std::shared_ptr<JsonNode> ExtensionService::descriptionsJson(
        const std::vector<ExtensionDescription>& list) {
    auto result = std::make_shared<JsonNode>(JsonNode::Type::Array);
    for (const auto& description : list) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);

        auto identifier = std::make_shared<JsonNode>(JsonNode::Type::Object);
        identifier->objectProperties.emplace_back(
            "value", vrutti::core::utils::makeJsonString(description.identifier));
        identifier->objectProperties.emplace_back(
            "_lower", vrutti::core::utils::makeJsonString(description.lowerIdentifier));
        identifier->objectProperties.emplace_back(
            "id", vrutti::core::utils::makeJsonString(description.identifier));
        node->objectProperties.emplace_back("identifier", identifier);

        node->objectProperties.emplace_back(
            "uuid", vrutti::core::utils::makeJsonString(description.uuid));
        node->objectProperties.emplace_back(
            "name", vrutti::core::utils::makeJsonString(description.name));
        node->objectProperties.emplace_back(
            "publisher", vrutti::core::utils::makeJsonString(description.publisher));
        node->objectProperties.emplace_back(
            "publisherDisplayName",
            vrutti::core::utils::makeJsonString(description.publisherDisplayName));
        node->objectProperties.emplace_back(
            "displayName", vrutti::core::utils::makeJsonString(description.displayName));
        node->objectProperties.emplace_back(
            "version", vrutti::core::utils::makeJsonString(description.version));

        auto engines = std::make_shared<JsonNode>(JsonNode::Type::Object);
        engines->objectProperties.emplace_back(
            "vscode", vrutti::core::utils::makeJsonString(description.enginesVscode));
        node->objectProperties.emplace_back("engines", engines);

        if (!description.description.empty()) {
            node->objectProperties.emplace_back(
                "description", vrutti::core::utils::makeJsonString(description.description));
        }
        if (!description.main.empty()) {
            node->objectProperties.emplace_back(
                "main", vrutti::core::utils::makeJsonString(description.main));
        }
        if (!description.browser.empty()) {
            node->objectProperties.emplace_back(
                "browser", vrutti::core::utils::makeJsonString(description.browser));
        }
        if (!description.activationEvents.empty()) {
            auto events = std::make_shared<JsonNode>(JsonNode::Type::Array);
            for (const auto& event : description.activationEvents) {
                events->arrayElements.push_back(vrutti::core::utils::makeJsonString(event));
            }
            node->objectProperties.emplace_back("activationEvents", events);
        }
        if (description.contributes) {
            node->objectProperties.emplace_back("contributes", description.contributes);
        }

        node->objectProperties.emplace_back(
            "extensionLocation", makeFileUri(fs::path(description.extensionLocation)));
        node->objectProperties.emplace_back(
            "targetPlatform", vrutti::core::utils::makeJsonString("undefined"));

        auto isBuiltin = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        isBuiltin->boolValue = description.isBuiltin;
        node->objectProperties.emplace_back("isBuiltin", isBuiltin);
        auto isUserBuiltin = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        isUserBuiltin->boolValue = false;
        node->objectProperties.emplace_back("isUserBuiltin", isUserBuiltin);
        auto isUnderDevelopment = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        isUnderDevelopment->boolValue = false;
        node->objectProperties.emplace_back("isUnderDevelopment", isUnderDevelopment);
        auto preRelease = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        preRelease->boolValue = false;
        node->objectProperties.emplace_back("preRelease", preRelease);

        result->arrayElements.push_back(node);
    }
    return result;
}

std::shared_ptr<JsonNode> ExtensionService::userInterfaceJson(
        const std::vector<ExtensionDescription>& list) {
    auto result = std::make_shared<JsonNode>(JsonNode::Type::Array);
    for (const auto& description : list) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);

        node->objectProperties.emplace_back(
            "id", vrutti::core::utils::makeJsonString(description.identifier));
        node->objectProperties.emplace_back(
            "name", vrutti::core::utils::makeJsonString(description.name));
        node->objectProperties.emplace_back(
            "displayName", vrutti::core::utils::makeJsonString(description.displayName));
        node->objectProperties.emplace_back(
            "publisherDisplayName",
            vrutti::core::utils::makeJsonString(description.publisherDisplayName));
        node->objectProperties.emplace_back(
            "description", vrutti::core::utils::makeJsonString(description.description));
        node->objectProperties.emplace_back(
            "version", vrutti::core::utils::makeJsonString(description.version));

        // The folder of the extension, with slashes: the interface builds
        // the address of the icon out of it.
        std::string localPath = description.extensionLocation;
        std::replace(localPath.begin(), localPath.end(), '\\', '/');
        node->objectProperties.emplace_back(
            "localPath", vrutti::core::utils::makeJsonString(localPath));

        bool isTheme = false;
        if (description.contributes && description.contributes->type == JsonNode::Type::Object) {
            if (description.contributes->get("themes") ||
                description.contributes->get("iconThemes")) {
                isTheme = true;
            }
            node->objectProperties.emplace_back("contributes", description.contributes);
        } else {
            auto empty = std::make_shared<JsonNode>(JsonNode::Type::Object);
            node->objectProperties.emplace_back("contributes", empty);
        }

        auto themeFlag = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        themeFlag->boolValue = isTheme;
        node->objectProperties.emplace_back("isTheme", themeFlag);

        auto enabledFlag = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        enabledFlag->boolValue = description.enabled;
        node->objectProperties.emplace_back("enabled", enabledFlag);

        auto builtinFlag = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        builtinFlag->boolValue = description.isBuiltin;
        node->objectProperties.emplace_back("isBuiltin", builtinFlag);

        result->arrayElements.push_back(node);
    }
    return result;
}

} // namespace vrutti::core::extensions
