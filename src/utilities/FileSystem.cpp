#include "utilities/FileSystem.h"

#include "utilities/StringUtilities.h"

#include <cstdlib>
#include <fstream>
#include <system_error>

namespace tnp::files {
namespace {

std::filesystem::path environmentPath(const char* variable) {
#if defined(_WIN32)
    // getenv is deprecated under MSVC's secure CRT; _dupenv_s owns the buffer.
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, variable) != 0 || buffer == nullptr) return {};
    std::filesystem::path result{buffer};
    std::free(buffer);
    return result;
#else
    const char* value = std::getenv(variable);
    return value ? std::filesystem::path{value} : std::filesystem::path{};
#endif
}

std::filesystem::path homeDirectory() {
#if defined(_WIN32)
    auto profile = environmentPath("USERPROFILE");
    if (!profile.empty()) return profile;
    return std::filesystem::current_path();
#else
    auto home = environmentPath("HOME");
    if (!home.empty()) return home;
    return std::filesystem::current_path();
#endif
}

std::filesystem::path ensureDirectory(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return path;
}

/// A temporary file next to the destination: renaming within one directory is
/// the only rename the standard library guarantees to be cheap and atomic.
std::filesystem::path temporaryNeighbour(const std::filesystem::path& path) {
    auto temp = path;
    temp += ".tnptmp";
    return temp;
}

Status atomicReplace(const std::filesystem::path& temp, const std::filesystem::path& target) {
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (!ec) return Status::ok();

    // Some Windows configurations refuse to rename onto an existing file when
    // an indexer holds a handle. Retry once through remove + rename.
    std::filesystem::remove(target, ec);
    std::error_code renameEc;
    std::filesystem::rename(temp, target, renameEc);
    if (renameEc) {
        std::filesystem::remove(temp, ec);
        return Status::failure(renameEc.message(), target.string());
    }
    return Status::ok();
}

} // namespace

std::filesystem::path userConfigDirectory() {
#if defined(_WIN32)
    auto base = environmentPath("APPDATA");
    if (base.empty()) base = homeDirectory();
    return ensureDirectory(base / "TNP");
#elif defined(__APPLE__)
    return ensureDirectory(homeDirectory() / "Library" / "Application Support" / "TNP");
#else
    auto base = environmentPath("XDG_CONFIG_HOME");
    if (base.empty()) base = homeDirectory() / ".config";
    return ensureDirectory(base / "tnp");
#endif
}

std::filesystem::path userStateDirectory() {
    return ensureDirectory(userConfigDirectory() / "state");
}

Result<std::string> readTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) return Result<std::string>::failure("cannot open file for reading", path.string());

    std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) return Result<std::string>::failure("read error", path.string());
    return content;
}

Result<ByteBuffer> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream) return Result<ByteBuffer>::failure("cannot open file for reading", path.string());

    ByteBuffer content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) return Result<ByteBuffer>::failure("read error", path.string());
    return content;
}

Status writeTextFileAtomic(const std::filesystem::path& path, std::string_view content) {
    const auto temp = temporaryNeighbour(path);
    {
        std::ofstream stream(temp, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream) return Status::failure("cannot open file for writing", temp.string());
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        stream.flush();
        if (!stream) return Status::failure("write error", temp.string());
    }
    return atomicReplace(temp, path);
}

Status writeBinaryFileAtomic(const std::filesystem::path& path, const ByteBuffer& content) {
    const auto temp = temporaryNeighbour(path);
    {
        std::ofstream stream(temp, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream) return Status::failure("cannot open file for writing", temp.string());
        if (!content.empty()) {
            stream.write(reinterpret_cast<const char*>(content.data()),
                         static_cast<std::streamsize>(content.size()));
        }
        stream.flush();
        if (!stream) return Status::failure("write error", temp.string());
    }
    return atomicReplace(temp, path);
}

std::string extensionOf(const std::filesystem::path& path) {
    return strings::toLower(path.extension().string());
}

} // namespace tnp::files
