#include "DetailArmorBarBedrock.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ui/SettingsMenu.h"
#include "render/ArmorHudRenderer.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <filesystem>
#include <exception>

namespace dab::bedrock {

namespace {

ll::event::ListenerPtr gSettingsKeyListener;

bool gF8WasDown = false;

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
    const auto armorAtlasArchive =
        getSelf().getConfigDir().parent_path() /
        "resources" / "DetailArmorBarBedrockTextures.zip";
    const bool hudInstalled =
        render::installArmorHudRenderer(mConfig, armorAtlasArchive);

    if (!hudInstalled) {
        getSelf().getLogger().error(
            "Failed to install armor HUD renderer."
        );

        return false;
    }

    // Poll the real Win32 virtual key from the game's UI render event. In GDK
    // client mode KeyInputEvent is not consistently delivered for F8 on every
    // launcher/build combination; edge detection prevents opening the form every
    // render frame while F8 is held down.
    gF8WasDown = false;
    gSettingsKeyListener =
        ll::event::EventBus::getInstance()
            .emplaceListener<ll::event::AfterUIRenderEvent>(
                [this](ll::event::AfterUIRenderEvent& event) {
                    const bool isDown = (::GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                    if (!isDown) {
                        gF8WasDown = false;
                        return;
                    }
                    if (gF8WasDown) {
                        return;
                    }
                    gF8WasDown = true;

                    auto& context = event.uiRenderContext();
                    auto* player = context.mClient.getLocalPlayer();
                    if (player == nullptr) {
                        getSelf().getLogger().warn(
                            "F8 was pressed but the local player is not available."
                        );
                        return;
                    }

                    getSelf().getLogger().info(
                        "F8 detected by Win32 polling; opening settings form."
                    );
                    try {
                        ui::openSettingsMenu(*player, mConfig, mConfigPath);
                    } catch (const std::exception& error) {
                        getSelf().getLogger().error(
                            "Failed to open F8 settings menu: {}", error.what()
                        );
                    } catch (...) {
                        getSelf().getLogger().error(
                            "Failed to open F8 settings menu: unknown exception."
                        );
                    }
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
    gF8WasDown = false;

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
