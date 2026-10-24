#pragma once

#include "config/ArmorBarConfig.h"
#include "ll/api/mod/Mod.h"
#include <filesystem>

namespace dab::bedrock {

class DetailArmorBarBedrock final : public ll::mod::Mod {
public:
    void load() override;
    bool enable() override;
    bool disable() override;

    static DetailArmorBarBedrock& getInstance();
    const config::ArmorBarConfig& getConfig() const { return mConfig; }
    config::ArmorBarConfig& getMutableConfig() { return mConfig; }
    const std::filesystem::path& getConfigPath() const { return mConfigPath; }

private:
    config::ArmorBarConfig mConfig{};
    std::filesystem::path mConfigPath;
};

} // namespace dab::bedrock
