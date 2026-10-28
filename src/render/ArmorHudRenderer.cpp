#include "ArmorHudRenderer.h"
#include "../feature/FeatureLogic.h"

#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/gui/controls/renderers/HudArmorRenderer.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/file/Path.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/renderer/BedrockTextureData.h"
#include "mc/deps/input/RectangleArea.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantUtils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <memory>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace dab::render {

namespace {

constexpr float kHalfPointWidth = 3.0f;
constexpr int kIconWidth = 8;
constexpr int kIconHeight = 9;
constexpr int kArmorPointsPerRow = 20;
constexpr int kIconsPerRow = 10;

// The vanilla armor row is left-aligned above the hearts in Bedrock HUD.
// These defaults target that row instead of drawing a second bar at screen center.
constexpr float kBaseXOffsetFromCenter = 91.0f;
constexpr int kBaseYFromBottom = 59;

config::ArmorBarConfig* gConfig = nullptr;
std::filesystem::path gArmorAtlasArchive;
std::shared_ptr<BedrockTextureData const> gArmorAtlasData;
bool gArmorAtlasLoadAttempted = false;
std::uint64_t gLastArmorAtlasAttemptFrame = 0;
bool gVanillaHudHookInstalled = false;
std::uint64_t gFrame = 0;

struct SlotHistory {
    std::string itemId;
    int lastDamage = -1;
    float lastPercent = -1.0f;
    int lastNotifiedThreshold = 101;
    std::uint64_t lastNotificationFrame = 0;

    std::array<bool, 4> thresholdTriggered{};
    std::array<std::chrono::steady_clock::time_point, 4>
        thresholdLastNotify{};
};

std::array<SlotHistory, 4> gHistory{};

float gLastHealth = -1.0f;
std::uint64_t gLastHurtFrame = 0;
std::uint64_t gLastMendingFrame = 0;

struct SlotInfo {
    SharedTypes::Legacy::ArmorSlot slot;
    const char* label;
};

// Read native ItemStack/enchantment data once per armor slot, then reuse it
// for every half-point drawn that frame. EnchantUtils calls are relatively
// expensive compared with the simple rectangles used for drawing.
struct PieceRenderState {
    bool equipped = false;
    std::string itemId;
    int armorPoints = 0;
    int damage = 0;
    int maxDamage = 0;
    int protection = 0;
    int projectileProtection = 0;
    int blastProtection = 0;
    int fireProtection = 0;
    bool mending = false;
    bool thorns = false;
    bool enchanted = false;
    bool hasProtection = false;
    feature::ProtectionKind protectionKind = feature::ProtectionKind::Generic;
    float durabilityWarningAlpha = 0.0f;
    std::uint32_t durabilityWarningColor = 0;
};

// Armor properties/enchantments are expensive native queries. Refresh them at
// a short interval and reuse the last snapshot on intermediate UI frames.
constexpr std::uint64_t kArmorStateRefreshIntervalFrames = 12;
std::array<PieceRenderState, 4> gPieceStates{};
feature::ArmorSnapshot gArmorSnapshot{};
Player* gCachedStatePlayer = nullptr;
std::uint64_t gLastStateRefreshFrame = 0;
bool gSnapshotValid = false;
std::vector<feature::BarPoint> gCachedPoints;
bool gLayoutCacheValid = false;
bool gLastToggleItemBar = false;
bool gLastToggleSortSpecialItem = false;

constexpr std::array<SlotInfo, 4> kArmorSlots{{
    {SharedTypes::Legacy::ArmorSlot::Head, "Helmet"},
    {SharedTypes::Legacy::ArmorSlot::Torso, "Chestplate"},
    {SharedTypes::Legacy::ArmorSlot::Legs, "Leggings"},
    {SharedTypes::Legacy::ArmorSlot::Feet, "Boots"},
}};

RectangleArea makeRect(
    float x,
    float y,
    float width,
    float height
) {
    RectangleArea rect;
    rect._x0 = x;
    rect._x1 = x + width;
    rect._y0 = y;
    rect._y1 = y + height;

    return rect;
}

mce::Color colorFromArgb(std::uint32_t argb) {
    constexpr float k = 1.0f / 255.0f;

    const auto r =
        static_cast<float>((argb >> 16) & 0xffu) * k;
    const auto g =
        static_cast<float>((argb >> 8) & 0xffu) * k;
    const auto b =
        static_cast<float>(argb & 0xffu) * k;

    return mce::Color(r, g, b, 1.0f);
}

float alphaFromArgb(std::uint32_t argb) {
    return static_cast<float>((argb >> 24) & 0xffu) / 255.0f;
}

void fill(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    float w,
    float h,
    std::uint32_t argb,
    float opacity = 1.0f
) {
    ctx.fillRectangle(
        makeRect(x, y, w, h),
        colorFromArgb(argb),
        std::clamp(
            alphaFromArgb(argb) * opacity,
            0.0f,
            1.0f
        )
    );
}

void outline(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    float w,
    float h,
    std::uint32_t argb,
    float opacity = 1.0f,
    int thickness = 1
) {
    ctx.drawRectangle(
        makeRect(x, y, w, h),
        colorFromArgb(argb),
        std::clamp(
            alphaFromArgb(argb) * opacity,
            0.0f,
            1.0f
        ),
        thickness
    );
}

// Load the Java port's 128x128 atlas from the ZIP packaged with the mod. The
// texture is held as BedrockTextureData so the UI renderer can reuse the same
// GPU texture on every subsequent frame (no per-frame texture lookup).
bool ensureArmorAtlas(MinecraftUIRenderContext& ctx) {
    if (gArmorAtlasData) {
        return true;
    }

    // A failed first lookup can happen before the client texture pipeline is
    // ready. Do not permanently lock the HUD into rectangle fallbacks; retry
    // at a low frequency so a missing archive does not add work every frame.
    constexpr std::uint64_t kAtlasRetryIntervalFrames = 120;
    if (gArmorAtlasLoadAttempted &&
        gFrame - gLastArmorAtlasAttemptFrame < kAtlasRetryIntervalFrames) {
        return false;
    }
    gArmorAtlasLoadAttempted = true;
    gLastArmorAtlasAttemptFrame = gFrame;

    std::error_code filesystemError;
    if (gArmorAtlasArchive.empty() ||
        !std::filesystem::exists(gArmorAtlasArchive, filesystemError)) {
        return false;
    }

    const Core::Path zipPath(gArmorAtlasArchive);
    const ResourceLocation atlasLocation{Core::PathView{"textures/armor_bar"}};
    auto texture = ctx.getZippedTexture(zipPath, atlasLocation, true);
    std::shared_ptr<BedrockTextureData const> textureData = texture.mClientTexture;
    if (!textureData) {
        // Some resource loaders expect the full archive entry name rather
        // than the texture's extensionless ResourceLocation.
        const ResourceLocation atlasLocationWithExtension{
            Core::PathView{"textures/armor_bar.png"}
        };
        auto textureWithExtension = ctx.getZippedTexture(
            zipPath, atlasLocationWithExtension, true
        );
        textureData = textureWithExtension.mClientTexture;
    }
    if (!textureData) {
        return false;
    }

    gArmorAtlasData = std::move(textureData);
    return true;
}

bool drawAtlasSprite(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    int u,
    int v,
    bool mirror = false
) {
    if (!ensureArmorAtlas(ctx)) {
        return false;
    }

    constexpr float kAtlasSize = 128.0f;
    constexpr float kSpriteSize = 9.0f;
    glm::vec2 uv(
        static_cast<float>(u) / kAtlasSize,
        static_cast<float>(v) / kAtlasSize
    );
    glm::vec2 uvSize(kSpriteSize / kAtlasSize, kSpriteSize / kAtlasSize);
    if (mirror) {
        uv.x += uvSize.x;
        uvSize.x = -uvSize.x;
    }

    ctx.drawImage(
        gArmorAtlasData->mClientTexture,
        glm::vec2(x, y),
        glm::vec2(kSpriteSize, kSpriteSize),
        uv,
        uvSize,
        false
    );
    return true;
}

std::pair<int, int> armorSpriteOffset(
    PieceRenderState const* piece,
    bool half,
    config::ArmorBarConfig const& c
) {
    // Exact offsets from Detail Armor Bar Reconstructed's armor_bar.png.
    // The second texture row is the vanilla-texture variant (+45 px).
    int u = half ? 54 : 63; // default iron/silver
    int v = 9;

    if (c.toggleArmorTypes && piece != nullptr && piece->equipped) {
        const std::string_view id(piece->itemId);

        if (id.find("netherite") != std::string_view::npos) {
            u = half ? 0 : 9;
        } else if (id.find("diamond") != std::string_view::npos) {
            u = half ? 18 : 27;
        } else if (id.find("turtle") != std::string_view::npos) {
            u = half ? 36 : 45;
        } else if (
            id.find("chainmail") != std::string_view::npos ||
            id.find("chain") != std::string_view::npos
        ) {
            u = half ? 72 : 81;
        } else if (
            id.find("golden") != std::string_view::npos ||
            id.find("gold") != std::string_view::npos
        ) {
            u = half ? 90 : 99;
        } else if (id.find("leather") != std::string_view::npos) {
            u = half ? 108 : 117;
        } else if (id.find("iron") != std::string_view::npos) {
            u = half ? 54 : 63;
        } else if (id.find("copper") != std::string_view::npos) {
            // Match Detail Armor Bar Reconstructed: copper uses the sprite
            // at U=0/9 and V=74, with the vanilla variant shifted by +45.
            u = half ? 0 : 9;
            v = 74;
        }
    }

    if (c.toggleVanillaTexture) {
        v += 45;
    }
    return {u, v};
}

PieceRenderState const* pieceForPoint(
    feature::BarPoint const* point,
    std::array<PieceRenderState, 4> const& states
) {
    if (point == nullptr || point->empty || point->pieceIndex < 0 ||
        point->pieceIndex >= static_cast<int>(states.size())) {
        return nullptr;
    }
    auto const& state = states[static_cast<std::size_t>(point->pieceIndex)];
    return state.equipped ? &state : nullptr;
}

std::pair<int, int> spriteOffsetForPoint(
    feature::BarPoint const* point,
    std::array<PieceRenderState, 4> const& states,
    bool half,
    config::ArmorBarConfig const& c
) {
    // Elytra/special-item slots use the same small item-bar sprite as Java.
    if (point != nullptr && !point->empty && point->specialItem) {
        return {36, 0};
    }
    return armorSpriteOffset(pieceForPoint(point, states), half, c);
}

std::uint32_t materialColor(std::string_view id, bool vanillaTexture);

std::uint32_t fallbackColorForPoint(
    feature::BarPoint const* point,
    std::array<PieceRenderState, 4> const& states,
    config::ArmorBarConfig const& c
) {
    auto const* piece = pieceForPoint(point, states);
    return piece != nullptr && c.toggleArmorTypes
        ? materialColor(piece->itemId, c.toggleVanillaTexture)
        : 0xffaeb6bfu;
}

void drawFallbackIcon(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    std::uint32_t color
);

void drawPointSprite(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    feature::BarPoint const* point,
    std::array<PieceRenderState, 4> const& states,
    config::ArmorBarConfig const& c,
    bool half,
    bool mirror
) {
    const auto [u, v] = spriteOffsetForPoint(point, states, half, c);
    if (drawAtlasSprite(ctx, x, y, u, v, mirror)) {
        return;
    }

    // Last-resort visible fallback only if the packaged atlas cannot be loaded.
    const auto color = fallbackColorForPoint(point, states, c);
    if (!half) {
        drawFallbackIcon(ctx, x, y, color);
        return;
    }
    const float left = mirror ? x + 5.0f : x + 1.0f;
    fill(ctx, left, y + 1.0f, 3.0f, 7.0f, color);
}

void drawEmptySprite(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    config::ArmorBarConfig const& c
) {
    if (!c.toggleEmptyBar) {
        return;
    }
    if (!drawAtlasSprite(ctx, x, y, 45, 0, false)) {
        // Empty sprite fallback is deliberately subdued rather than drawing a
        // second opaque background over the game HUD.
        outline(ctx, x, y, 8.0f, 9.0f, 0xff59636bu, 0.7f, 1);
    }
}

void drawFallbackIcon(
    MinecraftUIRenderContext& ctx,
    float x,
    float y,
    std::uint32_t color
) {
    // Only used when the atlas ZIP is missing/unreadable; the main render path
    // uses the original Java sprites above.
    fill(ctx, x + 1.0f, y + 1.0f, 7.0f, 7.0f, color);
    outline(ctx, x, y, 9.0f, 9.0f, 0xff20252au, 1.0f, 1);
}

Item const* itemOf(ItemStack const& stack) {
    return stack.isNull() ? nullptr : stack.mItem.get();
}

int armorValue(ItemStack const& stack) {
    auto const* item = itemOf(stack);

    return item
        ? std::max(0, static_cast<int>(item->getArmorValue()))
        : 0;
}

int maxDamage(ItemStack const& stack) {
    auto const* item = itemOf(stack);

    return item
        ? std::max(0, static_cast<int>(item->getMaxDamage()))
        : 0;
}

int enchantLevel(
    Enchant::Type type,
    ItemStack const& stack
) {
    return stack.isNull()
        ? 0
        : EnchantUtils::getEnchantLevel(type, stack);
}


std::uint32_t materialColor(
    std::string_view id,
    bool vanillaTexture
) {
    if (id.find("netherite") != std::string_view::npos) {
        return vanillaTexture ? 0xff81777du : 0xff66535cu;
    }

    if (id.find("diamond") != std::string_view::npos) {
        return vanillaTexture ? 0xff70c3c5u : 0xff55d9dcu;
    }

    if (
        id.find("golden") != std::string_view::npos ||
        id.find("gold") != std::string_view::npos
    ) {
        return vanillaTexture ? 0xffd9b75eu : 0xffffd34eu;
    }

    if (id.find("iron") != std::string_view::npos) {
        return vanillaTexture ? 0xffb9bec2u : 0xffc6c9cbu;
    }

    if (
        id.find("chainmail") != std::string_view::npos ||
        id.find("chain") != std::string_view::npos
    ) {
        return vanillaTexture ? 0xff89979bu : 0xff8e9b9eu;
    }

    if (id.find("leather") != std::string_view::npos) {
        return vanillaTexture ? 0xff8b6548u : 0xff986238u;
    }

    if (id.find("turtle") != std::string_view::npos) {
        return vanillaTexture ? 0xff4e8058u : 0xff41834du;
    }

    if (id.find("copper") != std::string_view::npos) {
        return vanillaTexture ? 0xffb97c5cu : 0xffc77b4eu;
    }

    return 0xff9dabb5u;
}

std::uint32_t durabilityColor(float percent) {
    if (percent <= 5.0f) {
        return 0xffff3333;
    }

    if (percent <= 10.0f) {
        return 0xffff5b22;
    }

    if (percent <= 25.0f) {
        return 0xffffa02c;
    }

    if (percent <= 50.0f) {
        return 0xffffd447;
    }

    return 0xff65d46eu;
}

void drawTinyDigit(
    MinecraftUIRenderContext& ctx,
    int digit,
    float x,
    float y,
    std::uint32_t color,
    float scale = 1.0f
) {
    static constexpr std::array<
        std::array<unsigned char, 5>,
        10
    > glyphs{{
        {{7, 5, 5, 5, 7}},
        {{2, 6, 2, 2, 7}},
        {{7, 1, 7, 4, 7}},
        {{7, 1, 7, 1, 7}},
        {{5, 5, 7, 1, 1}},
        {{7, 4, 7, 1, 7}},
        {{7, 4, 7, 5, 7}},
        {{7, 1, 1, 1, 1}},
        {{7, 5, 7, 5, 7}},
        {{7, 5, 7, 1, 7}}
    }};

    if (digit < 0 || digit > 9) {
        return;
    }

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (
                (
                    glyphs[static_cast<std::size_t>(digit)]
                          [static_cast<std::size_t>(row)] &
                    (1u << (2 - col))
                ) != 0
            ) {
                fill(
                    ctx,
                    x + static_cast<float>(col) * scale,
                    y + static_cast<float>(row) * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

void drawPercent(
    MinecraftUIRenderContext& ctx,
    int percent,
    float x,
    float y,
    float scale = 1.0f
) {
    percent = std::clamp(percent, 0, 100);

    if (percent == 100) {
        drawTinyDigit(ctx, 1, x, y, 0xffffffffu, scale);
        drawTinyDigit(
            ctx, 0, x + 4.0f * scale, y, 0xffffffffu, scale
        );
        drawTinyDigit(
            ctx, 0, x + 8.0f * scale, y, 0xffffffffu, scale
        );
    } else {
        drawTinyDigit(
            ctx, percent / 10, x, y, 0xffffffffu, scale
        );
        drawTinyDigit(
            ctx,
            percent % 10,
            x + 4.0f * scale,
            y,
            0xffffffffu,
            scale
        );
    }
}

float protectionPulse(
    std::string const& effectType,
    std::string const& speed,
    std::uint64_t frame
) {
    if (effectType == "STATIC" || effectType == "OUTLINE") {
        return 1.0f;
    }

    if (effectType != "AURA") {
        return 0.0f;
    }

    const int duration =
        feature::thornsAnimationDurationTicks(speed);

    const float phase =
        static_cast<float>(
            frame % static_cast<std::uint64_t>(duration * 2)
        ) /
        static_cast<float>(duration * 2);

    return 0.28f + 0.72f *
        (0.5f + 0.5f * std::sin(phase * 6.28318530718f));
}

int enabledThresholdAt(
    float percent,
    config::ArmorBarConfig const& c
) {
    if (c.toggleThreshold5 && percent <= 5.0f) {
        return 5;
    }

    if (c.toggleThreshold10 && percent <= 10.0f) {
        return 10;
    }

    if (c.toggleThreshold25 && percent <= 25.0f) {
        return 25;
    }

    if (c.toggleThreshold50 && percent <= 50.0f) {
        return 50;
    }

    return 101;
}

int thresholdIndex(int threshold) {
    switch (threshold) {
        case 50: return 0;
        case 25: return 1;
        case 10: return 2;
        case 5: return 3;
        default: return -1;
    }
}

void notifyDurability(
    IClientInstance& client,
    int damage,
    bool hasMending,
    int slotIndex,
    std::string const& itemId,
    float percent,
    config::ArmorBarConfig const& c
) {
    SlotHistory& history =
        gHistory[static_cast<std::size_t>(slotIndex)];

    const bool newItem = history.itemId != itemId;

    if (newItem) {
        history.thresholdTriggered.fill(false);
        history.lastPercent = -1.0f;
        history.lastDamage = -1;
    }

    if (
        !newItem &&
        history.lastDamage >= 0 &&
        damage < history.lastDamage &&
        hasMending
    ) {
        gLastMendingFrame = gFrame;
    }

    const std::array<int, 4> thresholds{50, 25, 10, 5};

    for (std::size_t i = 0; i < thresholds.size(); ++i) {
        if (percent > static_cast<float>(thresholds[i])) {
            history.thresholdTriggered[i] = false;
        }
    }

    const int threshold = enabledThresholdAt(percent, c);
    const int index = thresholdIndex(threshold);

    if (c.toggleDurabilityNotifications && index >= 0) {
        const auto now = std::chrono::steady_clock::now();

        const bool alreadyTriggered =
            history.thresholdTriggered[
                static_cast<std::size_t>(index)
            ];

        const auto lastTime =
            history.thresholdLastNotify[
                static_cast<std::size_t>(index)
            ];

        const bool cooldownPassed =
            lastTime.time_since_epoch().count() == 0 ||
            now - lastTime >= std::chrono::seconds(5);

        if (
            !alreadyTriggered ||
            (
                c.toggleRepeatedDurabilityNotifications &&
                cooldownPassed
            )
        ) {
            if (c.toggleDurabilityToastNotification) {
                auto gui = client.getGuiData();

                const std::string message =
                    "Durability warning (" +
                    std::to_string(threshold) +
                    "%): " +
                    std::string(
                        kArmorSlots[
                            static_cast<std::size_t>(slotIndex)
                        ].label
                    ) +
                    " " +
                    std::to_string(
                        static_cast<int>(std::round(percent))
                    ) +
                    "% remaining";

                gui->showPopupNotice(
                    message,
                    "Detail Armor Bar"
                );
            }

            if (c.toggleDurabilitySoundNotification) {
                switch (threshold) {
                    case 50:
                        client.playUI("note.hat", 0.2f, 1.0f);
                        break;
                    case 25:
                        client.playUI("note.pling", 0.3f, 1.0f);
                        break;
                    case 10:
                        client.playUI("note.bass", 0.4f, 1.0f);
                        break;
                    case 5:
                        client.playUI(
                            "random.anvil_land",
                            0.5f,
                            1.0f
                        );
                        break;
                    default:
                        break;
                }
            }

            history.thresholdTriggered[
                static_cast<std::size_t>(index)
            ] = true;

            history.thresholdLastNotify[
                static_cast<std::size_t>(index)
            ] = now;

            history.lastNotificationFrame = gFrame;
            history.lastNotifiedThreshold = threshold;
        }
    }

    history.itemId = itemId;
    history.lastDamage = damage;
    history.lastPercent = percent;
}

void renderDurabilityHud(
    MinecraftUIRenderContext& ctx,
    RectangleArea const& screen,
    std::array<PieceRenderState, 4> const& pieces,
    config::ArmorBarConfig const& c
) {
    if (!c.toggleDurability || !c.toggleDurabilityOverlay) {
        return;
    }

    const float left = screen._x0;
    const float right = screen._x1;
    const float top = screen._y0;
    const float bottom = screen._y1;

    const float scale =
        std::clamp(c.durabilityHudScale, 0.5f, 3.0f);
    const float panelWidth = 88.0f * scale;
    const float rowHeight = 15.0f * scale;

    float x = left + 8.0f;
    float y = top + 12.0f;

    if (c.durabilityHudPosition.find("RIGHT") != std::string::npos) {
        x = right - panelWidth - 8.0f;
    }

    if (c.durabilityHudPosition.find("BOTTOM") != std::string::npos) {
        y = bottom - rowHeight * 4.0f - 70.0f;
    }

    x += static_cast<float>(c.durabilityHudOffsetX);
    y += static_cast<float>(c.durabilityHudOffsetY);

    for (int i = 0; i < 4; ++i) {
        auto const& piece = pieces[static_cast<std::size_t>(i)];
        if (!piece.equipped || piece.maxDamage <= 0) {
            continue;
        }

        const int max = piece.maxDamage;
        const int damage = std::clamp(piece.damage, 0, max);

        const float percent = std::clamp(
            (
                1.0f -
                static_cast<float>(damage) /
                    static_cast<float>(max)
            ) * 100.0f,
            0.0f,
            100.0f
        );

        const float ry =
            y + static_cast<float>(i) * rowHeight;

        fill(
            ctx,
            x,
            ry,
            panelWidth,
            11.0f * scale,
            0xcc101418u
        );

        outline(
            ctx,
            x,
            ry,
            panelWidth,
            11.0f * scale,
            0xffc8d0d8u,
            0.8f,
            1
        );

        fill(
            ctx,
            x + 3.0f * scale,
            ry + 3.0f * scale,
            5.0f * scale,
            5.0f * scale,
            materialColor(
                piece.itemId,
                c.toggleVanillaTexture
            )
        );

        fill(
            ctx,
            x + 12.0f * scale,
            ry + 3.0f * scale,
            40.0f * scale,
            5.0f * scale,
            0xff3b4147u
        );

        fill(
            ctx,
            x + 12.0f * scale,
            ry + 3.0f * scale,
            40.0f * scale * percent / 100.0f,
            5.0f * scale,
            durabilityColor(percent)
        );

        drawPercent(
            ctx,
            static_cast<int>(std::round(percent)),
            x + 58.0f * scale,
            ry + 3.0f * scale,
            std::max(0.6f, 0.8f * scale)
        );
    }
}

void renderArmorBar(
    MinecraftUIRenderContext& ctx,
    RectangleArea const& screen,
    Player& player,
    IClientInstance& client,
    config::ArmorBarConfig const& c
) {
    const bool playerChanged = gCachedStatePlayer != &player;
    const bool refreshState =
        playerChanged || !gSnapshotValid ||
        (gFrame - gLastStateRefreshFrame >= kArmorStateRefreshIntervalFrames);

    if (playerChanged) {
        gHistory = {};
        gLastHealth = -1.0f;
        gLastHurtFrame = 0;
        gLastMendingFrame = 0;
        gSnapshotValid = false;
        gLayoutCacheValid = false;
    }

    if (refreshState) {
        gCachedStatePlayer = &player;
        gLastStateRefreshFrame = gFrame;
        gArmorSnapshot = feature::ArmorSnapshot{};

        int armorSum = 0;
        for (std::size_t i = 0; i < kArmorSlots.size(); ++i) {
            auto const& stack = player.getArmor(kArmorSlots[i].slot);
            auto& state = gPieceStates[i];
            state = PieceRenderState{};

            feature::ArmorPiece& piece = gArmorSnapshot.pieces[i];
            state.equipped = !stack.isNull();
            if (state.equipped) {
                state.itemId = stack.getRawNameId();
                state.armorPoints = armorValue(stack);
                state.damage = static_cast<int>(stack.getDamageValue());
                state.maxDamage = maxDamage(stack);
                state.protection = enchantLevel(Enchant::Type::Protection, stack);
                state.projectileProtection = enchantLevel(Enchant::Type::ProjectileProtection, stack);
                state.blastProtection = enchantLevel(Enchant::Type::BlastProtection, stack);
                state.fireProtection = enchantLevel(Enchant::Type::FireProtection, stack);
                state.mending = enchantLevel(Enchant::Type::Mending, stack) > 0;
                state.thorns = enchantLevel(Enchant::Type::Thorns, stack) > 0;
                state.enchanted = stack.isEnchanted();
                state.hasProtection = state.protection > 0 ||
                    state.projectileProtection > 0 ||
                    state.blastProtection > 0 ||
                    state.fireProtection > 0;
                state.protectionKind = state.protection > 0
                    ? feature::ProtectionKind::Generic
                    : state.projectileProtection > 0
                        ? feature::ProtectionKind::Projectile
                        : state.blastProtection > 0
                            ? feature::ProtectionKind::Blast
                            : feature::ProtectionKind::Fire;
            }

            piece.equipped = state.equipped;
            piece.itemId = state.itemId;
            piece.armorPoints = state.armorPoints;
            piece.damage = state.damage;
            piece.maxDamage = state.maxDamage;
            piece.mending = state.mending;
            piece.thorns = state.thorns;
            piece.enchanted = state.enchanted;
            // The layout builder checks toggleItemBar itself; don't bake a
            // transient config toggle into the cached snapshot.
            piece.hasCustomItemBar = state.equipped &&
                state.itemId.find("elytra") != std::string::npos;
            piece.customItemBarPoints = 2;
            armorSum += piece.armorPoints;

            if (state.equipped && state.maxDamage > 0) {
                const float percent = feature::durabilityPercentRemaining(
                    state.damage,
                    state.maxDamage
                );

                if (c.toggleDurability) {
                    notifyDurability(
                        client,
                        state.damage,
                        state.mending,
                        static_cast<int>(i),
                        state.itemId,
                        percent,
                        c
                    );
                } else {
                    if (
                        gHistory[i].itemId == state.itemId &&
                        gHistory[i].lastDamage >= 0 &&
                        state.damage < gHistory[i].lastDamage &&
                        state.mending
                    ) {
                        gLastMendingFrame = gFrame;
                    }

                    gHistory[i].itemId = state.itemId;
                    gHistory[i].lastDamage = state.damage;
                    gHistory[i].lastPercent = percent;
                }
            } else {
                gHistory[i] = {};
            }
        }

        gArmorSnapshot.reportedArmorValue =
            std::max(0, player.getArmorValue());
        gArmorSnapshot.baseArmor = std::max(
            0,
            gArmorSnapshot.reportedArmorValue - armorSum
        );
        gSnapshotValid = true;
        gLayoutCacheValid = false;
    }

    auto const& snapshot = gArmorSnapshot;
    auto const& pieceStates = gPieceStates;

    if (
        !gLayoutCacheValid ||
        gLastToggleItemBar != c.toggleItemBar ||
        gLastToggleSortSpecialItem != c.toggleSortSpecialItem
    ) {
        gCachedPoints = feature::buildBarPointLayout(snapshot, c);
        gLastToggleItemBar = c.toggleItemBar;
        gLastToggleSortSpecialItem = c.toggleSortSpecialItem;
        gLayoutCacheValid = true;
    }
    auto const& points = gCachedPoints;
    const int totalPoints = static_cast<int>(points.size());

    if (totalPoints == 0 && c.toggleHideBarWithoutArmor) {
        return;
    }

    const int intendedPoints = totalPoints == 0
        ? (c.toggleEmptyBar ? kArmorPointsPerRow : 0)
        : std::max(kArmorPointsPerRow, totalPoints);

    const int drawPoints = c.toggleStackArmorBars
        ? intendedPoints
        : std::min(intendedPoints, kArmorPointsPerRow);

    const int rows = c.toggleStackArmorBars
        ? std::max(
            1,
            (
                drawPoints + kArmorPointsPerRow - 1
            ) / kArmorPointsPerRow
        )
        : 1;

    const float screenLeft = screen._x0;
    const float screenRight = screen._x1;
    const float screenBottom = screen._y1;

    // Bedrock centers the hotbar, while the native armor points occupy the
    // left portion of the status HUD. Start the replacement row above hearts.
    // The config offsets remain available for different GUI scales/safe areas.
    const float baseX =
        (screenLeft + screenRight) * 0.5f -
        kBaseXOffsetFromCenter +
        static_cast<float>(c.armorBarOffsetX);

    const float baseY =
        screenBottom -
        static_cast<float>(kBaseYFromBottom) +
        static_cast<float>(c.armorBarOffsetY) -
        static_cast<float>(rows - 1) * 10.0f;

    bool anyProtection = false;
    std::array<std::uint32_t, 4> protectionColors{};
    for (std::size_t i = 0; i < pieceStates.size(); ++i) {
        auto const& state = pieceStates[i];
        anyProtection = anyProtection || state.hasProtection;

        protectionColors[i] = c.toggleUniformColor
            ? c.uniformColorArgb
            : feature::protectionColor(state.protectionKind, c);
    }


    std::vector<std::uint32_t> unalignedColors;
    // This data is only used by the unaligned-protection mode. Avoid allocating
    // and filling a vector every frame when the default aligned mode is enabled.
    if (c.toggleEnchants && !c.toggleAlignEnchantments) {
        int genericUnits = 0;
        int projectileUnits = 0;
        int blastUnits = 0;
        int fireUnits = 0;

        for (auto const& state : pieceStates) {
            genericUnits += state.protection + (state.protection > 0 ? 1 : 0);
            projectileUnits += state.projectileProtection;
            blastUnits += state.blastProtection;
            fireUnits += state.fireProtection;
        }

        unalignedColors.reserve(static_cast<std::size_t>(
            genericUnits + projectileUnits + blastUnits + fireUnits
        ));

        auto appendProtection = [&](
        feature::ProtectionKind kind,
        int count
    ) {
        const auto color = c.toggleUniformColor
            ? c.uniformColorArgb
            : feature::protectionColor(kind, c);

        for (int i = 0; i < std::max(0, count); ++i) {
            unalignedColors.push_back(color);
        }
    };

        appendProtection(feature::ProtectionKind::Generic, genericUnits);
        appendProtection(feature::ProtectionKind::Projectile, projectileUnits);
        appendProtection(feature::ProtectionKind::Blast, blastUnits);
        appendProtection(feature::ProtectionKind::Fire, fireUnits);
    }

    const float currentHealth = static_cast<float>(player.getHealth());
    if (gLastHealth >= 0.0f && currentHealth < gLastHealth) {
        gLastHurtFrame = gFrame;
    }
    gLastHealth = currentHealth;

    const float protectionOverlayPulse =
        protectionPulse(c.effectType, c.effectSpeed, gFrame);

    // Durability warning math (including the pulse sine) only needs to run once
    // per equipped piece, not once for every half-point of that piece.
    for (std::size_t i = 0; i < pieceStates.size(); ++i) {
        auto& state = gPieceStates[i];
        state.durabilityWarningAlpha = 0.0f;
        state.durabilityWarningColor = 0;

        if (
            !state.equipped || state.maxDamage <= 0 ||
            !c.toggleDurability || !c.toggleDurabilityNotifications ||
            !c.toggleDurabilityVisualEffect
        ) {
            continue;
        }

        const float percent = feature::durabilityPercentRemaining(
            state.damage, state.maxDamage
        );
        const auto warning = feature::durabilityWarning(percent, c);
        const auto lastNotice = gHistory[i].lastNotificationFrame;
        if (warning.level == feature::DurabilityLevel::None || lastNotice == 0) {
            continue;
        }

        const auto elapsed = gFrame - lastNotice;
        if (c.toggleRepeatedDurabilityNotifications) {
            state.durabilityWarningAlpha = 0.45f + 0.45f *
                (0.5f + 0.5f * std::sin(static_cast<float>(gFrame) * 0.22f));
        } else if (elapsed < 40) {
            state.durabilityWarningAlpha =
                1.0f - static_cast<float>(elapsed) / 40.0f;
        }
        state.durabilityWarningColor = warning.colorArgb;
    }

    auto pointAt = [&points](int index) -> feature::BarPoint const* {
        if (index < 0 || index >= static_cast<int>(points.size())) {
            return nullptr;
        }
        return &points[static_cast<std::size_t>(index)];
    };

    // Render ten 9x9 atlas sprites with an 8px stride, just like the Java mod.
    // This is the base HUD pass. Vanilla HudArmorRenderer is hooked below, so
    // it does not draw the default armor row on top of these sprites.
    for (int row = 0; row < rows; ++row) {
        for (int iconColumnInRow = 0; iconColumnInRow < kIconsPerRow; ++iconColumnInRow) {
            const int iconColumn = c.toggleInverseSlot
                ? kIconsPerRow - 1 - iconColumnInRow
                : iconColumnInRow;
            const float x = baseX + static_cast<float>(iconColumn * kIconWidth);
            const float y = baseY + static_cast<float>(row * 10);
            const int firstIndex = row * kArmorPointsPerRow + iconColumnInRow * 2;
            auto const* first = pointAt(firstIndex);
            auto const* second = pointAt(firstIndex + 1);
            const bool firstPresent = first != nullptr && !first->empty;
            const bool secondPresent = second != nullptr && !second->empty;

            if (firstPresent && secondPresent) {
                const auto firstHalf = spriteOffsetForPoint(first, pieceStates, true, c);
                const auto secondHalf = spriteOffsetForPoint(second, pieceStates, true, c);
                const bool sameBar = first->specialItem == second->specialItem &&
                                     firstHalf == secondHalf;
                if (sameBar) {
                    drawPointSprite(ctx, x, y, first, pieceStates, c, false, false);
                } else {
                    // Match Java: draw the right half mirrored first, then the
                    // left half. The atlas provides separate material halves.
                    drawPointSprite(ctx, x, y, second, pieceStates, c, true, true);
                    drawPointSprite(ctx, x, y, first, pieceStates, c, true, false);
                }
            } else if (firstPresent) {
                drawEmptySprite(ctx, x, y, c);
                drawPointSprite(ctx, x, y, first, pieceStates, c, true, false);
            } else if (secondPresent) {
                drawEmptySprite(ctx, x, y, c);
                drawPointSprite(ctx, x, y, second, pieceStates, c, true, true);
            } else {
                drawEmptySprite(ctx, x, y, c);
            }
        }
    }

    // Effects are grouped per 9x9 icon rather than redrawing a fill/outline for
    // every half-point. That is materially cheaper on low-end integrated GPUs.
    for (int row = 0; row < rows; ++row) {
        for (int iconColumnInRow = 0; iconColumnInRow < kIconsPerRow; ++iconColumnInRow) {
            const int iconColumn = c.toggleInverseSlot
                ? kIconsPerRow - 1 - iconColumnInRow
                : iconColumnInRow;
            const float x = baseX + static_cast<float>(iconColumn * kIconWidth);
            const float y = baseY + static_cast<float>(row * 10);
            const int firstIndex = row * kArmorPointsPerRow + iconColumnInRow * 2;
            auto const* first = pointAt(firstIndex);
            auto const* second = pointAt(firstIndex + 1);
            auto const* firstPiece = pieceForPoint(first, pieceStates);
            auto const* secondPiece = pieceForPoint(second, pieceStates);

            if ((first == nullptr || first->empty) &&
                (second == nullptr || second->empty)) {
                continue;
            }

            PieceRenderState const* effectPiece = firstPiece;
            if (effectPiece == nullptr || !effectPiece->hasProtection) {
                effectPiece = secondPiece;
            }
            if (effectPiece == nullptr) {
                continue;
            }

            bool hasProtectionOverlay = false;
            std::uint32_t protectionArgb = 0;
            if (c.toggleEnchants && c.effectType != "NONE" && anyProtection) {
                if (c.toggleUniformColor) {
                    hasProtectionOverlay = effectPiece->hasProtection;
                    protectionArgb = c.uniformColorArgb;
                } else if (effectPiece->hasProtection) {
                    hasProtectionOverlay = true;
                    protectionArgb = protectionColors[
                        static_cast<std::size_t>(effectPiece - pieceStates.data())
                    ];
                }
            }

            if (hasProtectionOverlay) {
                if (c.effectType == "OUTLINE") {
                    outline(ctx, x, y, 9.0f, 9.0f,
                            protectionArgb, protectionOverlayPulse, 1);
                } else if (c.effectType == "AURA" || c.effectType == "STATIC") {
                    fill(ctx, x + 1.0f, y + 1.0f, 7.0f, 7.0f,
                         protectionArgb, c.effectType == "AURA"
                             ? std::max(0.12f, protectionOverlayPulse * 0.30f)
                             : 0.20f);
                }
            }

            const bool hasThorns =
                (firstPiece != nullptr && firstPiece->thorns) ||
                (secondPiece != nullptr && secondPiece->thorns);
            if (c.toggleThorns && hasThorns) {
                float thornAlpha = 1.0f;
                std::uint32_t thornColor = 0xffffffffu;
                const int duration = feature::thornsAnimationDurationTicks(c.effectSpeed);
                const auto elapsed = gFrame - gLastHurtFrame;
                if (c.effectThorn == "ANIMATION" && gLastHurtFrame != 0 &&
                    elapsed < static_cast<std::uint64_t>(duration)) {
                    thornColor = elapsed < static_cast<std::uint64_t>(duration / 2)
                        ? 0xffff3b3bu : 0xffffa0a0u;
                    thornAlpha = 0.45f + 0.55f *
                        static_cast<float>(duration - static_cast<int>(elapsed)) /
                        static_cast<float>(duration);
                }
                outline(ctx, x, y, 9.0f, 9.0f, thornColor, thornAlpha, 1);
            }

            const bool hasMending =
                (firstPiece != nullptr && firstPiece->mending) ||
                (secondPiece != nullptr && secondPiece->mending);
            if (c.toggleMending && hasMending && (gFrame % 16) < 9) {
                const float sparkleX = x + 4.0f;
                fill(ctx, sparkleX, y - 1.0f, 1.0f, 3.0f, 0xffffffffu);
                fill(ctx, sparkleX - 1.0f, y, 3.0f, 1.0f, 0xffffffffu);
            }
            if (c.toggleMending && hasMending && gLastMendingFrame != 0 &&
                gFrame - gLastMendingFrame < 12) {
                const float flash = (gFrame - gLastMendingFrame) % 6 < 3 ? 1.0f : 0.35f;
                outline(ctx, x, y, 9.0f, 9.0f, 0xffffffffu, flash, 1);
            }

            PieceRenderState const* durabilityPiece = nullptr;
            for (auto const* candidate : {firstPiece, secondPiece}) {
                if (candidate != nullptr && candidate->durabilityWarningAlpha > 0.0f &&
                    (durabilityPiece == nullptr ||
                     candidate->durabilityWarningAlpha > durabilityPiece->durabilityWarningAlpha)) {
                    durabilityPiece = candidate;
                }
            }
            if (c.toggleDurabilityVisualEffect && durabilityPiece != nullptr) {
                outline(ctx, x, y, 9.0f, 9.0f,
                        durabilityPiece->durabilityWarningColor,
                        durabilityPiece->durabilityWarningAlpha, 1);
            }
        }
    }

    if (
        !c.toggleAlignEnchantments &&
        c.toggleEnchants &&
        c.effectType != "NONE" &&
        !unalignedColors.empty()
    ) {
        const int n = std::min(
            drawPoints,
            static_cast<int>(unalignedColors.size())
        );

        for (int pointIndex = 0; pointIndex < n; ++pointIndex) {
            const int row = pointIndex / kArmorPointsPerRow;
            const int inRow = pointIndex % kArmorPointsPerRow;

            int iconColumn = inRow / 2;
            const bool secondHalf = (inRow % 2) != 0;

            if (c.toggleInverseSlot) {
                iconColumn = kIconsPerRow - 1 - iconColumn;
            }

            const float x =
                baseX + static_cast<float>(iconColumn * kIconWidth);
            const float y =
                baseY + static_cast<float>(row * 10);

            const float halfX =
                x + (secondHalf != c.toggleInverseSlot ? 4.0f : 1.0f);
            const float w = static_cast<float>(kHalfPointWidth);

            const auto color =
                unalignedColors[static_cast<std::size_t>(pointIndex)];

            const float pulse = protectionOverlayPulse;

            if (c.effectType == "OUTLINE") {
                outline(ctx, halfX, y + 1.0f, w, 7.0f, color, pulse, 1);
            } else {
                fill(ctx, halfX, y + 1.0f, w, 7.0f, color, pulse);
            }
        }
    }

    if (
        totalPoints > kArmorPointsPerRow &&
        !c.toggleStackArmorBars
    ) {
        const int overflowRows =
            (totalPoints - 1) / kArmorPointsPerRow;

        for (int i = 0; i < overflowRows; ++i) {
            const float shift = static_cast<float>((i + 1) * 3);

            const float x = c.toggleInverseOverflowIcon
                ? baseX + 80.0f - shift
                : baseX + 80.0f + shift;

            const float y = baseY + 1.0f;

            fill(ctx, x, y, 3.0f, 3.0f, 0xffffd34eu);
            fill(ctx, x, y + 4.0f, 3.0f, 3.0f, 0xffffd34eu);
        }
    }

    renderDurabilityHud(ctx, screen, pieceStates, c);
}

} // namespace

// Replace the game's exact armor HUD render call. Unlike AfterUIRenderEvent,
// this fires only for the native armor row, so the mod does not repaint HUDs for
// unrelated screens or use a changing clipping rectangle from other UI layers.
LL_TYPE_INSTANCE_HOOK(
    VanillaHudArmorRendererHook,
    ll::memory::HookPriority::High,
    HudArmorRenderer,
    &HudArmorRenderer::$render,
    void,
    MinecraftUIRenderContext& renderContext,
    IClientInstance& client,
    UIControl& owner,
    int pass
) {
    (void)owner;
    (void)pass;
    auto* player = client.getLocalPlayer();
    if (gConfig == nullptr || player == nullptr) {
        return;
    }
    ++gFrame;
    // HudArmorRenderer can be invoked under a control-local scissor. Temporarily
    // use the full HUD viewport so the replacement atlas row is not clipped to
    // the vanilla control's original glyph bounds.
    renderContext.saveCurrentClippingRectangle();
    renderContext.setFullClippingRectangle();
    const auto screen = renderContext.getFullClippingRectangle();
    renderArmorBar(renderContext, screen, *player, client, *gConfig);
    renderContext.restoreSavedClippingRectangle();
}

bool installArmorHudRenderer(
    config::ArmorBarConfig& config,
    std::filesystem::path armorAtlasArchive
) {
    gConfig = &config;
    gArmorAtlasArchive = std::move(armorAtlasArchive);
    gArmorAtlasData.reset();
    gArmorAtlasLoadAttempted = false;
    gLastArmorAtlasAttemptFrame = 0;

    if (!gVanillaHudHookInstalled) {
        VanillaHudArmorRendererHook::hook();
        gVanillaHudHookInstalled = true;
    }
    return gVanillaHudHookInstalled;
}

void uninstallArmorHudRenderer() {
    if (gVanillaHudHookInstalled) {
        VanillaHudArmorRendererHook::unhook();
        gVanillaHudHookInstalled = false;
    }

    gConfig = nullptr;
    gCachedStatePlayer = nullptr;
    gLastStateRefreshFrame = 0;
    gSnapshotValid = false;
    gLayoutCacheValid = false;
    gPieceStates = {};
    gArmorSnapshot = feature::ArmorSnapshot{};
    gCachedPoints.clear();
    gFrame = 0;
    gHistory = {};
    gLastHealth = -1.0f;
    gLastHurtFrame = 0;
    gLastMendingFrame = 0;
    gArmorAtlasData.reset();
    gArmorAtlasArchive.clear();
    gArmorAtlasLoadAttempted = false;
    gLastArmorAtlasAttemptFrame = 0;
}

} // namespace dab::render
