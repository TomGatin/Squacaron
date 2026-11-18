#include "assets/AssetPath.hpp"

#include <string_view>

namespace assets {

bool isValidAssetName(const std::string_view name) noexcept {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH && !name.contains('\0');
}

bool isValidAssetAlias(const std::string_view alias) noexcept {
    return isValidAssetName(alias) && !looksLikeAssetPath(alias);
}

bool looksLikeAssetPath(const std::string_view aliasOrPath) noexcept {
    return aliasOrPath.find_first_of("/\\.") != std::string_view::npos;
}

}  // namespace assets
