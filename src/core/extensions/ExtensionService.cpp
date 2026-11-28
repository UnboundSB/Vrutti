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
#include <vector>

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

    // Compares two versions.
    int compareVersions(int majorA, int minorA, int patchA,
                        int majorB, int minorB, int patchB) {
        if (majorA != majorB) return majorA < majorB ? -1 : 1;
        if (minorA != minorB) return minorA < minorB ? -1 : 1;
        if (patchA != patchB) return patchA < patchB ? -1 : 1;
        return 0;
    }

    // Compares two versions that are written as text.
    //
    // A version that cannot be read is the smallest one: a folder with a
    // broken name does not take the place of a folder with a good one.
    int compareVersionText(const std::string& left, const std::string& right) {
        int lmajor = 0, lminor = 0, lpatch = 0;
        int rmajor = 0, rminor = 0, rpatch = 0;
        const bool leftOk = parseVersion(left, lmajor, lminor, lpatch);
        const bool rightOk = parseVersion(right, rmajor, rminor, rpatch);
        if (!leftOk && !rightOk) return 0;
        if (!leftOk) return -1;
        if (!rightOk) return 1;
        return compareVersions(lmajor, lminor, lpatch, rmajor, rminor, rpatch);
    }

    // Reads one part of a range of versions.
    //
    // The part is one of the shapes that the manifests use:
    //
    //   `^1.2.3`   the next big version is not allowed
    //   `~1.2.3`   the next small version is not allowed
    //   `>=1.2.3`  the version has to be at least this one
    //   `>1.2.3`   the version has to be newer than this one
    //   `<=1.2.3`  the version has to be at most this one
    //   `<1.2.3`   the version has to be older than this one
    //   `=1.2.3`   the version has to be this one
    //   `1.2.3`    the same as `=1.2.3`
    //   `*`        every version is allowed
    //
    // Returns false when the part is a shape that is not known. A range that
    // is not known is accepted later on: an extension that asks for
    // something strange is better refused by the extension host than by a
    // guess in the installer.
    enum class RangeKind { Any, Exact, Greater, GreaterOrEqual, Less, LessOrEqual, Caret, Tilde };

    bool parseRangePart(const std::string& text, RangeKind& kind,
                        int& major, int& minor, int& patch) {
        major = minor = patch = 0;
        kind = RangeKind::Any;

        std::string rest = text;
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(rest.begin());
        }
        if (rest.empty()) return true;
        if (rest == "*" || rest == "x" || rest == "X") return true;

        if (rest[0] == '^') { kind = RangeKind::Caret; rest.erase(rest.begin()); }
        else if (rest[0] == '~') { kind = RangeKind::Tilde; rest.erase(rest.begin()); }
        else if (rest[0] == '>') {
            kind = RangeKind::Greater;
            rest.erase(rest.begin());
            if (!rest.empty() && rest[0] == '=') { kind = RangeKind::GreaterOrEqual; rest.erase(rest.begin()); }
        } else if (rest[0] == '<') {
            kind = RangeKind::Less;
            rest.erase(rest.begin());
            if (!rest.empty() && rest[0] == '=') { kind = RangeKind::LessOrEqual; rest.erase(rest.begin()); }
        } else if (rest[0] == '=') {
            kind = RangeKind::Exact;
            rest.erase(rest.begin());
        } else {
            kind = RangeKind::Exact;
        }

        // The text after the sign may still have spaces in it.
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(rest.begin());
        }
        if (rest.empty()) return false;

        // A part that is only a big number means "this big version and
        // everything that is under it".
        const size_t dot = rest.find('.');
        if (dot == std::string::npos) {
            int value = 0;
            if (!parseVersion(rest, value, minor, patch)) return false;
            if (minor != 0 || patch != 0) return false;
            major = value;
            if (kind == RangeKind::Exact) kind = RangeKind::Caret;
            return true;
        }

        return parseVersion(rest, major, minor, patch);
    }

    // Tells whether a version is inside of a range.
    //
    // The range is a list of parts that all have to hold, separated by
    // spaces. The parts are separated by `||` into alternatives: one of the
    // alternatives has to hold.
    bool versionMatchesRange(const std::string& range,
                             int hostMajor, int hostMinor, int hostPatch) {
        // The alternatives are separated by `||`.
        std::vector<std::string> alternatives;
        {
            std::string rest = range;
            size_t pos = 0;
            while (true) {
                const size_t bar = rest.find("||", pos);
                if (bar == std::string::npos) {
                    alternatives.push_back(rest.substr(pos));
                    break;
                }
                alternatives.push_back(rest.substr(pos, bar - pos));
                pos = bar + 2;
            }
        }

        for (const auto& alternative : alternatives) {
            // The parts of one alternative are separated by spaces.
            std::vector<std::string> parts;
            {
                std::istringstream stream(alternative);
                std::string part;
                while (stream >> part) parts.push_back(part);
            }
            if (parts.empty()) {
                // An empty alternative allows every version.
                return true;
            }

            bool allHold = true;
            bool allKnown = true;
            for (const auto& part : parts) {
                RangeKind kind = RangeKind::Any;
                int major = 0, minor = 0, patch = 0;
                if (!parseRangePart(part, kind, major, minor, patch)) {
                    allKnown = false;
                    break;
                }

                const int order = compareVersions(hostMajor, hostMinor, hostPatch,
                                                  major, minor, patch);
                bool holds = true;
                switch (kind) {
                    case RangeKind::Any:
                        holds = true;
                        break;
                    case RangeKind::Exact:
                        holds = order == 0;
                        break;
                    case RangeKind::Greater:
                        holds = order > 0;
                        break;
                    case RangeKind::GreaterOrEqual:
                        holds = order >= 0;
                        break;
                    case RangeKind::Less:
                        holds = order < 0;
                        break;
                    case RangeKind::LessOrEqual:
                        holds = order <= 0;
                        break;
                    case RangeKind::Caret: {
                        // The next big version is not allowed. A version
                        // that starts with a zero is special: the next
                        // small version is not allowed either.
                        holds = order >= 0;
                        if (holds) {
                            if (major > 0) {
                                holds = hostMajor == major;
                            } else if (minor > 0) {
                                holds = hostMajor == 0 && hostMinor == minor;
                            } else {
                                holds = hostMajor == 0 && hostMinor == 0 && hostPatch == patch;
                            }
                        }
                        break;
                    }
                    case RangeKind::Tilde: {
                        // The next small version is not allowed.
                        holds = order >= 0 && hostMajor == major && hostMinor == minor;
                        break;
                    }
                }
                if (!holds) {
                    allHold = false;
                    break;
                }
            }

            if (!allKnown) {
                // A shape that is not known: the range cannot be read, so
                // the extension is let through.
                return true;
            }
            if (allHold) return true;
        }
        return false;
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
    // Does this extension answer to the name that was given?
    //
    // The name is already in lower case. A name without a publisher asks
    // for the extension of that name, whatever the publisher is.
    bool matchesIdentifier(const ExtensionDescription& description,
                           const std::string& wanted) {
        if (wanted.empty()) return false;
        if (description.lowerIdentifier == wanted) return true;
        if (wanted.find('.') == std::string::npos) {
            return toLower(description.name) == wanted;
        }
        return false;
    }

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

    // A range that Vrutti cannot read is accepted: an extension that asks
    // for something strange is better refused by the extension host than by
    // a guess in the installer.
    int hostMajor = 0, hostMinor = 0, hostPatch = 0;
    if (!parseVersion(hostVersion(), hostMajor, hostMinor, hostPatch)) return true;

    if (!versionMatchesRange(enginesVscode, hostMajor, hostMinor, hostPatch)) {
        error = "the extension asks for version " + enginesVscode +
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
        // The entry point has to be a file inside of the extension.
        //
        // A manifest may name a path that walks out of the extension
        // ("../outside.js") or a path that is absolute. The extension host
        // would load that file, and the extension would run code that is
        // somewhere else on the disk: the archive that was installed is not
        // the thing that runs.
        std::error_code entryError;
        const fs::path relative(entryPoint);
        if (relative.is_absolute() || relative.has_root_name() ||
            relative.has_root_directory()) {
            fs::remove_all(staging, ec);
            error = "the entry point of the extension is outside of the extension: " +
                    entryPoint;
            errorCode = "InvalidExtension";
            return false;
        }

        const fs::path normalized = relative.lexically_normal();
        const bool walksOut = normalized.empty() || normalized == ".." ||
                              (*normalized.begin() == "..");
        if (walksOut) {
            fs::remove_all(staging, ec);
            error = "the entry point of the extension is outside of the extension: " +
                    entryPoint;
            errorCode = "InvalidExtension";
            return false;
        }

        const fs::path entryPath = staging / normalized;
        if (!fs::exists(entryPath, entryError) || fs::is_directory(entryPath, entryError)) {
            fs::remove_all(staging, ec);
            error = "the entry point of the extension is missing: " + entryPoint;
            errorCode = "InvalidExtension";
            return false;
        }

        // The entry point has to be inside of the extension even when the
        // folder of the extension holds a link that points somewhere else.
        fs::path resolvedStaging = fs::weakly_canonical(staging, entryError);
        if (entryError) resolvedStaging = fs::canonical(staging, entryError);
        fs::path resolvedEntry = fs::weakly_canonical(entryPath, entryError);
        if (entryError) resolvedEntry = fs::canonical(entryPath, entryError);
        if (!entryError) {
            const std::string root = resolvedStaging.generic_string();
            const std::string entry = resolvedEntry.generic_string();
            const bool inside = entry.size() > root.size() &&
                                entry.compare(0, root.size(), root) == 0 &&
                                (root.back() == '/' || root.back() == '\\' ||
                                 entry[root.size()] == '/' || entry[root.size()] == '\\');
            if (!inside) {
                fs::remove_all(staging, ec);
                error = "the entry point of the extension is outside of the extension: " +
                        entryPoint;
                errorCode = "InvalidExtension";
                return false;
            }
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

    // An extension that ships with the editor is not removed: it is part of
    // the editor, and removing it would leave the editor broken.
    for (const auto& description : builtinFoldersOnDisk()) {
        if (!matchesIdentifier(description, wanted)) continue;
        error = "the extension is built into the editor";
        errorCode = "InvalidExtension";
        return false;
    }

    // The folders of the extension on the disk.
    //
    // The list the user sees is made of the disk, so the removal is made of
    // the disk too: an extension that is in a folder but not in the
    // metadata is still installed, and the user may take it away. Before,
    // the list showed it and the removal said that it was not installed.
    //
    // Every copy of the extension is taken away: two folders may hold the
    // same extension, and one of them would stay behind and show up again.
    std::vector<std::string> removedFolders;
    for (const auto& description : userFoldersOnDisk()) {
        if (!matchesIdentifier(description, wanted)) continue;

        // The name of the folder has to be a single part of a name. A
        // metadata file that was edited by hand must not be able to point
        // outside of the folder of the extensions.
        const std::string& location = description.relativeLocation;
        const bool safe = !location.empty() &&
            location.find('/') == std::string::npos &&
            location.find('\\') == std::string::npos &&
            location != "." && location != "..";
        if (!safe) continue;

        std::error_code ec;
        fs::remove_all(userExtensionsDirectory() / location, ec);
        if (ec) {
            error = "the folder of the extension could not be removed: " + ec.message();
            errorCode = "Unavailable";
            return false;
        }
        removedFolders.push_back(location);
    }

    // The entry of the metadata.
    auto installedList = readMetadata();
    std::vector<ExtensionDescription> remaining;
    bool foundInMetadata = false;
    for (const auto& entry : installedList) {
        if (matchesIdentifier(entry, wanted)) {
            foundInMetadata = true;
            continue;
        }
        remaining.push_back(entry);
    }

    if (removedFolders.empty() && !foundInMetadata) {
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

    auto installedList = readMetadata();
    bool found = false;
    for (auto& entry : installedList) {
        if (matchesIdentifier(entry, wanted)) {
            entry.enabled = enabled;
            found = true;
            break;
        }
    }

    if (!found) {
        // The extension is on the disk but not in the metadata: it was put
        // there by hand, or by an older version of the editor. It is added
        // to the metadata, so that its state is kept from now on. Before,
        // the list showed it and the switch said that it was not installed.
        for (const auto& description : userFoldersOnDisk()) {
            if (!matchesIdentifier(description, wanted)) continue;
            auto entry = description;
            entry.enabled = enabled;
            if (entry.uuid.empty()) entry.uuid = stableUuid(entry.identifier);
            installedList.push_back(entry);
            found = true;
            break;
        }
    }

    if (!found) {
        for (const auto& description : builtinFoldersOnDisk()) {
            if (!matchesIdentifier(description, wanted)) continue;
            error = "the extension is built into the editor";
            errorCode = "InvalidExtension";
            return false;
        }
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

std::vector<ExtensionDescription> ExtensionService::userFoldersOnDisk() {
    std::vector<ExtensionDescription> result;
    std::error_code ec;

    const fs::path userDir = userExtensionsDirectory();
    if (!fs::exists(userDir, ec) || !fs::is_directory(userDir, ec)) return result;

    for (const auto& entry : fs::directory_iterator(userDir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;

        ExtensionDescription description;
        if (!describeFolder(entry.path(), false, description)) continue;
        result.push_back(description);
    }
    return result;
}

std::vector<ExtensionDescription> ExtensionService::builtinFoldersOnDisk() {
    std::vector<ExtensionDescription> result;
    std::error_code ec;

    const fs::path builtinDir = builtinExtensionsDirectory();
    if (!fs::exists(builtinDir, ec) || !fs::is_directory(builtinDir, ec)) return result;

    for (const auto& entry : fs::directory_iterator(builtinDir, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;

        ExtensionDescription description;
        if (!describeFolder(entry.path(), true, description)) continue;
        result.push_back(description);
    }
    return result;
}

std::vector<ExtensionDescription> ExtensionService::listAll() {
    std::vector<ExtensionDescription> result;

    // The state of the extensions of the user comes from the metadata file:
    // it knows which ones are turned off.
    auto metadata = readMetadata();

    // The folder the metadata points at, for every extension it knows.
    auto knownFolder = [&](const std::string& lowerIdentifier) -> std::string {
        for (const auto& known : metadata) {
            if (known.lowerIdentifier == lowerIdentifier) return known.relativeLocation;
        }
        return "";
    };

    // The extensions that ship with the editor.
    for (const auto& description : builtinFoldersOnDisk()) {
        result.push_back(description);
    }

    // The extensions of the user.
    //
    // Two folders may hold the same extension: a copy that was made by
    // hand, or a folder that an older version of the editor left behind.
    // Only one of them is shown -- the folder the metadata points at, or
    // the newest one -- so that the list of the user does not show the same
    // extension twice.
    std::vector<ExtensionDescription> chosen;
    for (const auto& description : userFoldersOnDisk()) {
        bool replaced = false;
        for (auto& kept : chosen) {
            if (kept.lowerIdentifier != description.lowerIdentifier) continue;
            replaced = true;

            const std::string wanted = knownFolder(description.lowerIdentifier);
            const bool keptIsKnown = !wanted.empty() &&
                kept.relativeLocation == wanted;
            const bool thisIsKnown = !wanted.empty() &&
                description.relativeLocation == wanted;

            if (thisIsKnown && !keptIsKnown) {
                kept = description;
            } else if (thisIsKnown == keptIsKnown &&
                       compareVersionText(description.version, kept.version) > 0) {
                kept = description;
            }
            break;
        }
        if (!replaced) chosen.push_back(description);
    }

    for (auto& description : chosen) {
        // The metadata file has the last word about the state of an
        // extension: it knows that one was turned off.
        for (const auto& known : metadata) {
            if (known.lowerIdentifier == description.lowerIdentifier) {
                description.enabled = known.enabled;
                break;
            }
        }
        result.push_back(description);
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
