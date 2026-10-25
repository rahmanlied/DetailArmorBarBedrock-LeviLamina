#include "DetailArmorBarBedrock.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

#include "ui/SettingsMenu.h"
#include "render/ArmorHudRenderer.h"

#include <filesystem>
#include <exception>

namespace dab::bedrock {

namespace {

ll::event::ListenerPtr gSettingsKeyListener;

constexpr int kSettingsKeyF8 = 0x77;

} // namespace

DetailArmorBarBedrock::DetailArmorBarBedrock()
    : mSelf(*ll::mod::NativeMod::current()) {
}

DetailArmorBarBedrock& DetailArmorBarBedrock::getInstance() {
    static DetailArmorBarBedrock instance;
    return instance;
}

bool DetailArmorBarBedrock::load() {
    try {
        mConfigPath = getSelf().getConfigDir() / "config.json";

        std::filesystem::create_directories(
            getSelf().getConfigDir()
        );

        mConfig = config::ArmorBarConfig::loadOrCreate(
            mConfigPath
        );

        getSelf().getLogger().info(
            "DetailArmorBarBedrock config loaded: {}",
            mConfigPath.string()
        );

        return true;
    } catch (const std::exception& error) {
        getSelf().getLogger().error(
            "Failed to load config: {}",
            error.what()
        );

        return false;
    } catch (...) {
        getSelf().getLogger().error(
            "Failed to load config due to an unknown error."
        );

        return false;
    }
}

bool DetailArmorBarBedrock::enable() {
    const bool hudInstalled =
        render::installArmorHudRenderer(mConfig);

    if (!hudInstalled) {
        getSelf().getLogger().error(
            "Failed to install armor HUD renderer."
        );

        return false;
    }

    gSettingsKeyListener =
        ll::event::EventBus::getInstance()
            .emplaceListener<ll::event::KeyInputEvent>(
                [this](ll::event::KeyInputEvent& event) {
                    static bool f8Held = false;

                    if (event.keyCode() != kSettingsKeyF8) {
                        return;
                    }

                    if (!event.isDown()) {
                        f8Held = false;
                        return;
                    }

                    if (f8Held) {
                        return;
                    }

                    f8Held = true;

                    auto* player =
                        event.controller().getLocalPlayer();

                    if (player == nullptr) {
                        return;
                    }

                    ui::openSettingsMenu(
                        *player,
                        mConfig,
                        mConfigPath
                    );
                }
            );

    if (!gSettingsKeyListener) {
        render::uninstallArmorHudRenderer();

        getSelf().getLogger().error(
            "Failed to register F8 settings listener."
        );

        return false;
    }

    getSelf().getLogger().info(
        "DetailArmorBarBedrock enabled."
    );

    return true;
}

bool DetailArmorBarBedrock::disable() {
    render::uninstallArmorHudRenderer();

    if (gSettingsKeyListener) {
        ll::event::EventBus::getInstance().removeListener(
            gSettingsKeyListener
        );

        gSettingsKeyListener.reset();
    }

    getSelf().getLogger().info(
        "DetailArmorBarBedrock disabled."
    );

    return true;
}

} // namespace dab::bedrock

LL_REGISTER_MOD(
    dab::bedrock::DetailArmorBarBedrock,
    dab::bedrock::DetailArmorBarBedrock::getInstance()
);
