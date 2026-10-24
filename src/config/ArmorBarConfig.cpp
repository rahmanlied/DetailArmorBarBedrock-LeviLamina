#include "ArmorBarConfig.h"

#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace dab::config {
namespace {
using json = nlohmann::json;

template <typename T>
void readIf(const json& source, const char* key, T& value) {
    const auto it = source.find(key);
    if (it == source.end() || it->is_null()) return;
    try { value = it->get<T>(); } catch (...) { /* keep default for malformed fields */ }
}

json toJson(const ArmorBarConfig& c) {
    return json{
        {"effectType", c.effectType}, {"effectSpeed", c.effectSpeed}, {"effectThorn", c.effectThorn},
        {"toggleEnchants", c.toggleEnchants}, {"toggleArmorTypes", c.toggleArmorTypes},
        {"toggleThorns", c.toggleThorns}, {"toggleDurability", c.toggleDurability},
        {"toggleMending", c.toggleMending}, {"toggleEmptyBar", c.toggleEmptyBar},
        {"toggleItemBar", c.toggleItemBar}, {"toggleHideBarWithoutArmor", c.toggleHideBarWithoutArmor},
        {"toggleVanillaTexture", c.toggleVanillaTexture}, {"toggleCompatibleHeartMod", c.toggleCompatibleHeartMod},
        {"toggleInverseSlot", c.toggleInverseSlot}, {"toggleInverseOverflowIcon", c.toggleInverseOverflowIcon},
        {"toggleStackArmorBars", c.toggleStackArmorBars}, {"toggleSortSpecialItem", c.toggleSortSpecialItem},
        {"toggleAlignEnchantments", c.toggleAlignEnchantments}, {"toggleUniformColor", c.toggleUniformColor},
        {"uniformColorArgb", c.uniformColorArgb}, {"protectionColorGenericArgb", c.protectionColorGenericArgb},
        {"protectionColorProjectileArgb", c.protectionColorProjectileArgb}, {"protectionColorBlastArgb", c.protectionColorBlastArgb},
        {"protectionColorFireArgb", c.protectionColorFireArgb}, {"uniformColorType", c.uniformColorType},
        {"armorBarOffsetX", c.armorBarOffsetX}, {"armorBarOffsetY", c.armorBarOffsetY},
        {"toggleArmorTrims", c.toggleArmorTrims}, {"toggleDurabilityOverlay", c.toggleDurabilityOverlay},
        {"durabilityHudPosition", c.durabilityHudPosition}, {"durabilityHudOffsetX", c.durabilityHudOffsetX},
        {"durabilityHudOffsetY", c.durabilityHudOffsetY}, {"durabilityHudScale", c.durabilityHudScale},
        {"toggleDurabilityNotifications", c.toggleDurabilityNotifications},
        {"toggleRepeatedDurabilityNotifications", c.toggleRepeatedDurabilityNotifications},
        {"toggleDurabilitySoundNotification", c.toggleDurabilitySoundNotification},
        {"toggleDurabilityToastNotification", c.toggleDurabilityToastNotification},
        {"toggleDurabilityVisualEffect", c.toggleDurabilityVisualEffect}, {"toggleThreshold50", c.toggleThreshold50},
        {"toggleThreshold25", c.toggleThreshold25}, {"toggleThreshold10", c.toggleThreshold10},
        {"toggleThreshold5", c.toggleThreshold5}, {"toggleInventoryOverlay", c.toggleInventoryOverlay}
    };
}

void fromJson(const json& j, ArmorBarConfig& c) {
#define DAB_READ(name) readIf(j, #name, c.name)
    DAB_READ(effectType); DAB_READ(effectSpeed); DAB_READ(effectThorn);
    DAB_READ(toggleEnchants); DAB_READ(toggleArmorTypes); DAB_READ(toggleThorns); DAB_READ(toggleDurability);
    DAB_READ(toggleMending); DAB_READ(toggleEmptyBar); DAB_READ(toggleItemBar); DAB_READ(toggleHideBarWithoutArmor);
    DAB_READ(toggleVanillaTexture); DAB_READ(toggleCompatibleHeartMod); DAB_READ(toggleInverseSlot);
    DAB_READ(toggleInverseOverflowIcon); DAB_READ(toggleStackArmorBars); DAB_READ(toggleSortSpecialItem);
    DAB_READ(toggleAlignEnchantments); DAB_READ(toggleUniformColor); DAB_READ(uniformColorArgb);
    DAB_READ(protectionColorGenericArgb); DAB_READ(protectionColorProjectileArgb); DAB_READ(protectionColorBlastArgb);
    DAB_READ(protectionColorFireArgb); DAB_READ(uniformColorType); DAB_READ(armorBarOffsetX); DAB_READ(armorBarOffsetY);
    DAB_READ(toggleArmorTrims); DAB_READ(toggleDurabilityOverlay); DAB_READ(durabilityHudPosition);
    DAB_READ(durabilityHudOffsetX); DAB_READ(durabilityHudOffsetY); DAB_READ(durabilityHudScale);
    DAB_READ(toggleDurabilityNotifications); DAB_READ(toggleRepeatedDurabilityNotifications);
    DAB_READ(toggleDurabilitySoundNotification); DAB_READ(toggleDurabilityToastNotification);
    DAB_READ(toggleDurabilityVisualEffect); DAB_READ(toggleThreshold50); DAB_READ(toggleThreshold25);
    DAB_READ(toggleThreshold10); DAB_READ(toggleThreshold5); DAB_READ(toggleInventoryOverlay);
#undef DAB_READ
}
} // namespace

ArmorBarConfig ArmorBarConfig::loadOrCreate(const std::filesystem::path& path) {
    ArmorBarConfig config;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!std::filesystem::exists(path)) {
        config.save(path);
        return config;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) return config;
    try {
        json parsed;
        in >> parsed;
        if (parsed.is_object()) fromJson(parsed, config);
    } catch (...) {
        // Never prevent the client from loading because a user config is damaged.
        config.save(path.string() + ".defaults.json");
    }
    return config;
}

void ArmorBarConfig::save(const std::filesystem::path& path) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << toJson(*this).dump(4) << '\n';
}

} // namespace dab::config
