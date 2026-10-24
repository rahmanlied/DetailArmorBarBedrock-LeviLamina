#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace dab::config {

// This mirrors Detail Armor Bar Reconstructed's public option names and defaults.
// Values that are not directly implementable via Java/Fabric APIs are retained so
// the Bedrock port can preserve configuration compatibility while features land.
struct ArmorBarConfig {
    std::string effectType = "AURA";              // NONE, AURA, OUTLINE, STATIC
    std::string effectSpeed = "NORMAL";           // VERY_SLOW, SLOW, NORMAL, FAST, VERY_FAST
    std::string effectThorn = "ANIMATION";        // STATIC, ANIMATION

    bool toggleEnchants = true;
    bool toggleArmorTypes = true;
    bool toggleThorns = true;
    bool toggleDurability = true;
    bool toggleMending = true;
    bool toggleEmptyBar = true;
    bool toggleItemBar = true;
    bool toggleHideBarWithoutArmor = true;
    bool toggleVanillaTexture = true;
    bool toggleCompatibleHeartMod = false;
    bool toggleInverseSlot = false;
    bool toggleInverseOverflowIcon = false;
    bool toggleStackArmorBars = false;
    bool toggleSortSpecialItem = true;
    bool toggleAlignEnchantments = true;
    bool toggleUniformColor = false;

    std::uint32_t uniformColorArgb = 0x5099FFFFu;
    std::uint32_t protectionColorGenericArgb = 0x5099FFFFu;
    std::uint32_t protectionColorProjectileArgb = 0x507033ADu;
    std::uint32_t protectionColorBlastArgb = 0x50FFFF00u;
    std::uint32_t protectionColorFireArgb = 0x50D23800u;
    std::string uniformColorType = "AQUA";

    int armorBarOffsetX = 0;
    int armorBarOffsetY = 0;

    bool toggleArmorTrims = true;
    bool toggleDurabilityOverlay = false;
    std::string durabilityHudPosition = "BOTTOM_LEFT";
    int durabilityHudOffsetX = 0;
    int durabilityHudOffsetY = 0;
    float durabilityHudScale = 1.0f;

    bool toggleDurabilityNotifications = true;
    bool toggleRepeatedDurabilityNotifications = false;
    bool toggleDurabilitySoundNotification = true;
    bool toggleDurabilityToastNotification = true;
    bool toggleDurabilityVisualEffect = true;
    bool toggleThreshold50 = false;
    bool toggleThreshold25 = true;
    bool toggleThreshold10 = true;
    bool toggleThreshold5 = true;
    bool toggleInventoryOverlay = false;

    static ArmorBarConfig loadOrCreate(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

} // namespace dab::config
