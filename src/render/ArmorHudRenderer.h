#pragma once

#include "../config/ArmorBarConfig.h"

namespace dab::render {

// Installs the local-client UI render adapter. It reads the player's equipped armor
// and draws the Bedrock-native version of the detail armor HUD.
bool installArmorHudRenderer(config::ArmorBarConfig& config);
void uninstallArmorHudRenderer();

} // namespace dab::render
