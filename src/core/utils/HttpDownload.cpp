#include "HttpDownload.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace vrutti::core::utils {

namespace {

    // The name of the folder of the temporary files of the app.
    fs::path temporaryDirectory() {
#ifdef _WIN32
        const char* temp = std::getenv("TEMP");
        if (temp && *temp) return fs::path(temp);
        const char* tmp = std::getenv("TMP");
        if (tmp && *tmp) return fs::path(tmp);
#else
        const char* tmp = std::getenv("TMPDIR");
        if (tmp && *tmp) return fs::path(tmp);
#endif
        return fs::temp_directory_path();
    }

    std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

} // namespace

bool HttpDownload::isAllowedUrl(const std::string& url) {
    const std::string lower = toLower(url);
    if (lower.rfind("https://", 0) == 0) return true;
    if (lower.rfind("http://", 0) == 0) {
        // The local machine may be served without a certificate.
        const std::string host = lower.substr(7);
        const size_t slash = host.find('/');
        const std::string authority = slash == std::string::npos ? host : host.substr(0, slash);
        return authority == "localhost" || authority == "127.0.0.1" ||
               authority == "[::1]" || authority.rfind("127.", 0) == 0;
    }
    return false;
}

std::string HttpDownload::uniqueFileName(const std::string& prefix,
                                         const std::string& suffix) {
    static int counter = 0;
    std::ostringstream out;
#ifdef _WIN32
    out << prefix << "-" << GetCurrentProcessId();
#else
    out << prefix << "-" << getpid();
#endif
    out << "-" << (++counter) << suffix;
    return out.str();
}

bool HttpDownload::download(const std::string& url,
                            const std::string& destinationDirectory,
                            std::string& destinationPath,
                            size_t maxBytes,
                            std::string& error) {
    destinationPath.clear();

    if (!isAllowedUrl(url)) {
        error = "the address of the file is not allowed: only https is allowed";
        return false;
    }

    std::error_code ec;
    fs::path directory(destinationDirectory);
    if (directory.empty()) directory = temporaryDirectory();
    fs::create_directories(directory, ec);

    const fs::path target = directory / uniqueFileName("vrutti-download", ".vsix");

#ifdef _WIN32
    HINTERNET session = InternetOpenA("VruttiIDE/1.0", INTERNET_OPEN_TYPE_PRECONFIG,
                                      nullptr, nullptr, 0);
    if (!session) {
        error = "the network could not be opened";
        return false;
    }

    HINTERNET connection = InternetOpenUrlA(session, url.c_str(), nullptr, 0,
                                             INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE |
                                             INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!connection) {
        InternetCloseHandle(session);
        error = "the file could not be fetched";
        return false;
    }

    HANDLE file = CreateFileA(target.string().c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(connection);
        InternetCloseHandle(session);
        error = "the file could not be written";
        return false;
    }

    char buffer[65536];
    size_t total = 0;
    bool ok = true;
    DWORD read = 0;
    while (InternetReadFile(connection, buffer, sizeof(buffer), &read) && read > 0) {
        if (total + read > maxBytes) {
            ok = false;
            error = "the file is bigger than the limit";
            break;
        }
        DWORD written = 0;
        if (!WriteFile(file, buffer, read, &written, nullptr) || written != read) {
            ok = false;
            error = "the file could not be written";
            break;
        }
        total += read;
    }

    CloseHandle(file);
    InternetCloseHandle(connection);
    InternetCloseHandle(session);

    if (!ok) {
        fs::remove(target, ec);
        return false;
    }
#else
    // On the other systems the file is fetched with curl. The address is an
    // argument of the program, never a part of a command, so an address with
    // a quote in it cannot do anything but be an address.
    const std::string targetPath = target.string();
    const pid_t pid = fork();
    if (pid < 0) {
        error = "the download could not be started";
        return false;
    }
    if (pid == 0) {
        const std::string sizeLimit = std::to_string(maxBytes);
        const char* argv[] = {
            "curl", "--location", "--fail", "--silent", "--show-error",
            "--max-time", "300", "--max-filesize", sizeLimit.c_str(),
            "--output", targetPath.c_str(), url.c_str(), nullptr};
        execvp("curl", const_cast<char* const*>(argv));
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fs::remove(target, ec);
        error = "the file could not be fetched";
        return false;
    }

    std::error_code sizeError;
    const uintmax_t size = fs::file_size(target, sizeError);
    if (sizeError || size > maxBytes) {
        fs::remove(target, ec);
        error = "the file is bigger than the limit";
        return false;
    }
#endif

    destinationPath = target.string();
    return true;
}

} // namespace vrutti::core::utils
