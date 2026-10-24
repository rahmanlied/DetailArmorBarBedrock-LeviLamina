#include "DetailArmorBarBedrock.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "ui/SettingsMenu.h"
#include "render/ArmorHudRenderer.h"

#include <filesystem>

namespace dab::bedrock {
namespace {
ll::event::ListenerPtr gSettingsKeyListener;
constexpr int kSettingsKeyF8 = 0x77; // Windows virtual-key code for F8
}

DetailArmorBarBedrock& DetailArmorBarBedrock::getInstance() {
    static DetailArmorBarBedrock instance;
    return instance;
}

void DetailArmorBarBedrock::load() {
    // Load the upstream-compatible options before installing the client HUD renderer.
    mConfigPath = getSelf().getConfigDir() / "config.json";
    mConfig = config::ArmorBarConfig::loadOrCreate(mConfigPath);
}

bool DetailArmorBarBedrock::enable() {
    // F8 opens the local client settings form. This uses the client-only key input
    // event; it does not send a command to the remote server.
    const bool hudInstalled = render::installArmorHudRenderer(mConfig);
    gSettingsKeyListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::KeyInputEvent>(
        [this](ll::event::KeyInputEvent& event) {
            static bool f8Held = false;
            if (event.keyCode() != kSettingsKeyF8) return;
            if (!event.isDown()) {
                f8Held = false;
                return;
            }
            if (f8Held) return; // Ignore keyboard repeat while the key is held.
            f8Held = true;
            auto* player = event.controller().getLocalPlayer();
            if (player == nullptr) return; // key can be pressed at the title screen
            ui::openSettingsMenu(*player, mConfig, mConfigPath);
        }
    );
    return hudInstalled && static_cast<bool>(gSettingsKeyListener);
}

bool DetailArmorBarBedrock::disable() {
    render::uninstallArmorHudRenderer();
    if (gSettingsKeyListener) {
        ll::event::EventBus::getInstance().removeListener(gSettingsKeyListener);
        gSettingsKeyListener.reset();
    }
    return true;
}

} // namespace dab::bedrock

LL_REGISTER_MOD(dab::bedrock::DetailArmorBarBedrock, dab::bedrock::DetailArmorBarBedrock::getInstance());
