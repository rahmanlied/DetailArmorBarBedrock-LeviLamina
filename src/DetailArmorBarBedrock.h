#pragma once

#include "config/ArmorBarConfig.h"
#include "ll/api/mod/NativeMod.h"

#include <filesystem>

namespace dab::bedrock {

class DetailArmorBarBedrock final {
public:
    DetailArmorBarBedrock();

    static DetailArmorBarBedrock& getInstance();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const {
        return mSelf;
    }

    bool load();
    bool enable();
    bool disable();

    [[nodiscard]] const config::ArmorBarConfig& getConfig() const {
        return mConfig;
    }

    [[nodiscard]] config::ArmorBarConfig& getMutableConfig() {
        return mConfig;
    }

    [[nodiscard]] const std::filesystem::path& getConfigPath() const {
        return mConfigPath;
    }

private:
    ll::mod::NativeMod& mSelf;

    config::ArmorBarConfig mConfig{};
    std::filesystem::path mConfigPath;
};

} // namespace dab::bedrock
