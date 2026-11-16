#include "utils/Paths.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace sq::utils {

std::optional<std::string> normalizePath(const std::string_view path) {
    if (path.empty() || path.size() > MAX_PATH_LENGTH || path.contains('\0')) {
        return std::nullopt;
    }
    std::u8string generic(path.begin(), path.end());
    std::ranges::replace(generic, u8'\\', u8'/');
    // Built from UTF-8 explicitly: a `std::string` would be read in the ANSI code page on Windows.
    std::string normalized = toUtf8(std::filesystem::path(generic).lexically_normal());
    while (normalized.size() > 1 && normalized.ends_with('/')) {
        normalized.pop_back();
    }
    if (normalized.empty() || normalized == ".") {
        return std::nullopt;
    }
    return normalized;
}

bool isConfinedPath(const std::string_view normalized) noexcept {
    if (normalized.empty() || normalized.starts_with('/')) {
        return false;
    }
    // A drive letter ("C:/", "c:x"): absolute or drive-relative on Windows, never inside the working directory.
    if (normalized.size() >= 2 && normalized[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(normalized[0])) != 0) {
        return false;
    }
    return normalized != ".." && !normalized.starts_with("../");
}

std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return {text.begin(), text.end()};
}

std::filesystem::path fromUtf8(const std::string_view text) { return {std::u8string(text.begin(), text.end())}; }

std::string quoteForLog(const std::string_view text, const std::size_t limit) {
    if (text.size() <= limit) {
        return "'" + std::string(text) + "'";
    }
    return "'" + std::string(text.substr(0, limit)) + "...'";
}

}  // namespace sq::utils
