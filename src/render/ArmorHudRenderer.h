#pragma once

#include "../config/ArmorBarConfig.h"

#include <filesystem>

namespace dab::render {

// Hooks the native Bedrock armor HUD renderer and replaces its icons with the
// Detail Armor Bar atlas. The archive must contain textures/armor_bar.png.
bool installArmorHudRenderer(
    config::ArmorBarConfig& config,
    std::filesystem::path armorAtlasArchive
);
void uninstallArmorHudRenderer();


} // namespace dab::render
