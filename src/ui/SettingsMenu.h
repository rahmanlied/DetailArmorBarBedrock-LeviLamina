#pragma once
#include "../config/ArmorBarConfig.h"
#include <filesystem>
class Player;
namespace dab::ui {
void openSettingsMenu(Player& player, config::ArmorBarConfig& config, std::filesystem::path configPath);
}
