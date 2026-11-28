#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/utils/Json.h"

namespace vrutti::core::extensions {

    // -------------------------------------------------------------------------
    // The description of an installed extension.
    //
    // The shape is the one of the upstream `IExtensionDescription`: the
    // extension host builds its registry out of it, and an extension that is
    // not described correctly does not activate.
    // -------------------------------------------------------------------------
    struct ExtensionDescription {
        std::string identifier;          // "publisher.name"
        std::string lowerIdentifier;     // the same, in lower case
        std::string uuid;                // the stable identity of the extension
        std::string name;
        std::string publisher;
        std::string publisherDisplayName;
        std::string displayName;
        std::string description;
        std::string version;
        std::string enginesVscode;       // the version range the extension asks for
        std::string main;                // the entry point of the extension
        std::string browser;             // the entry point for the browser
        std::string extensionLocation;   // the folder of the extension
        std::string relativeLocation;    // the folder, relative to the extensions folder
        std::vector<std::string> activationEvents;
        std::shared_ptr<vrutti::core::utils::JsonNode> contributes;
        bool isBuiltin = false;
        bool enabled = true;
    };

    // -------------------------------------------------------------------------
    // The service that installs and removes extensions.
    //
    // The service is the one place that knows what is installed: the list it
    // gives is the list the extension host gets, and the list the user
    // interface shows. When the two are made by different code, they drift
    // apart, and an extension that the user sees is not the one that runs.
    //
    // The layout is the one of the upstream editor:
    //
    //     <extensions folder>/<publisher>.<name>-<version>/
    //         package.json
    //         <the files of the extension>
    //
    // and the file `<extensions folder>/extensions.json` remembers what is
    // installed and what is turned off.
    // -------------------------------------------------------------------------
    class ExtensionService {
    public:
        // The folder that holds the extensions of the user.
        //
        // The environment variable `VRUTTI_EXTENSIONS_DIR` wins over the
        // default, so that a test can use a folder of its own.
        static std::filesystem::path userExtensionsDirectory();

        // The folder that holds the extensions that ship with the app.
        static std::filesystem::path builtinExtensionsDirectory();

        // The file that remembers what is installed.
        static std::filesystem::path metadataFile();

        // Installs a `.vsix` file.
        //
        // The archive is checked before anything is written: it has to hold
        // a manifest, the manifest has to name the extension, and the version
        // the extension asks for has to be one that the host can run.
        //
        // The extension is unpacked into a folder of its own, and the
        // folders of the older versions of the same extension are removed.
        //
        // Returns false when the file is not an extension that can be
        // installed. The reason is in `error`, the code of the failure in
        // `errorCode` (for example "InvalidExtension" or "FileNotFound").
        static bool installVsix(const std::string& vsixPath,
                                ExtensionDescription& installed,
                                std::string& error,
                                std::string& errorCode);

        // Removes an extension: its folder and its entry in the metadata.
        static bool uninstall(const std::string& identifier,
                              std::string& error,
                              std::string& errorCode);

        // Turns an extension on or off. A turned off extension stays on the
        // disk, but it is not given to the extension host.
        static bool setEnabled(const std::string& identifier,
                               bool enabled,
                               std::string& error,
                               std::string& errorCode);

        // Every extension that is installed, turned off ones included.
        static std::vector<ExtensionDescription> listAll();

        // Every extension that is installed and turned on.
        static std::vector<ExtensionDescription> listEnabled();

        // The descriptions in the shape of the upstream `IExtensionDescription`.
        static std::shared_ptr<vrutti::core::utils::JsonNode>
        descriptionsJson(const std::vector<ExtensionDescription>& list);

        // The list in the shape that the user interface expects.
        //
        // The interface shows the name of the extension, its publisher, its
        // icon and what it brings to the editor. It is a different shape than
        // the one of the host, and it is built here so that the list of the
        // interface and the list of the host come from the same source.
        static std::shared_ptr<vrutti::core::utils::JsonNode>
        userInterfaceJson(const std::vector<ExtensionDescription>& list);

        // Reads the manifest of a folder that holds an extension.
        static bool readManifest(const std::filesystem::path& extensionRoot,
                                 ExtensionDescription& out,
                                 std::string& error);

        // Checks the version range of an extension against the version of the
        // host. An empty range means "any version".
        static bool isCompatible(const std::string& enginesVscode,
                                 std::string& error);

        // The version of the host that runs the extensions.
        static const char* hostVersion();

        // A stable identity of an extension, the same for every run.
        static std::string stableUuid(const std::string& seed);

    private:
        // Reads the metadata file. A missing file means "nothing is
        // installed", which is not an error.
        static std::vector<ExtensionDescription> readMetadata();

        // Writes the metadata file.
        static bool writeMetadata(const std::vector<ExtensionDescription>& list,
                                  std::string& error);

        // Removes the folders of the older versions of an extension.
        static void removeOlderVersions(const std::string& identifier,
                                        const std::string& keepFolder);

        // The description of an extension that is on the disk.
        static bool describeFolder(const std::filesystem::path& root,
                                   bool isBuiltin,
                                   ExtensionDescription& out);

        // Every extension of the user that is on the disk.
        //
        // The list is made of the disk and not of the metadata file: a
        // folder that was put there by hand, or that an older version of the
        // editor left behind, holds an extension that is installed. The
        // metadata has the last word about whether it is turned on.
        static std::vector<ExtensionDescription> userFoldersOnDisk();

        // Every extension that ships with the editor.
        static std::vector<ExtensionDescription> builtinFoldersOnDisk();
    };

} // namespace vrutti::core::extensions
