#pragma once

#include <cstddef>
#include <string>

namespace vrutti::core::utils {

    // -------------------------------------------------------------------------
    // The download of a file.
    //
    // The downloader is strict on purpose:
    //
    //   * only `https` is allowed (and `http` for the local machine),
    //   * the file may not be bigger than the limit that the caller gives,
    //   * the file is written with a name that no other process uses, so two
    //     downloads never fight over the same file,
    //   * a download that fails leaves nothing behind.
    //
    // There is no shell anywhere: the address of the file is an argument of
    // the program, never a part of a command.
    // -------------------------------------------------------------------------
    class HttpDownload {
    public:
        // Downloads a file into a folder.
        //
        // The path of the file that was written is in `destinationPath`. The
        // file is *not* removed by the downloader: the caller owns it, and
        // removes it when it is done with it.
        //
        // Returns false when the file could not be downloaded. The reason is
        // in `error`.
        static bool download(const std::string& url,
                             const std::string& destinationDirectory,
                             std::string& destinationPath,
                             size_t maxBytes,
                             std::string& error);

        // True when the address may be downloaded: `https` everywhere, and
        // `http` for the local machine.
        static bool isAllowedUrl(const std::string& url);

        // The name of a file that no other process uses.
        static std::string uniqueFileName(const std::string& prefix,
                                          const std::string& suffix);
    };

} // namespace vrutti::core::utils
