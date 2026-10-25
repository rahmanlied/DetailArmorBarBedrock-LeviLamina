#include "ArmorHudRenderer.h"
#include "../feature/FeatureLogic.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/GuiData.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/math/Color.h"
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
#include <string>
#include <string_view>
#include <vector>

namespace dab::render {
namespace {

constexpr int kHalfPointWidth = 4;
constexpr int kIconWidth = 9;
constexpr int kIconHeight = 9;
constexpr int kArmorPointsPerRow = 20;
constexpr int kIconsPerRow = 10;
constexpr int kBaseYFromBottom = 80;

ll::event::ListenerPtr gHudListener;
config::ArmorBarConfig* gConfig = nullptr;
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

bool hasProtection(ItemStack const& stack) {
    return
        enchantLevel(Enchant::Type::Protection, stack) > 0 ||
        enchantLevel(Enchant::Type::ProjectileProtection, stack) > 0 ||
        enchantLevel(Enchant::Type::BlastProtection, stack) > 0 ||
        enchantLevel(Enchant::Type::FireProtection, stack) > 0;
}

feature::ProtectionKind protectionKind(ItemStack const& stack) {
    if (enchantLevel(Enchant::Type::Protection, stack) > 0) {
        return feature::ProtectionKind::Generic;
    }

    if (enchantLevel(Enchant::Type::ProjectileProtection, stack) > 0) {
        return feature::ProtectionKind::Projectile;
    }

    if (enchantLevel(Enchant::Type::BlastProtection, stack) > 0) {
        return feature::ProtectionKind::Blast;
    }

    return feature::ProtectionKind::Fire;
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
    ItemStack const& stack,
    int slotIndex,
    std::string const& itemId,
    float percent,
    config::ArmorBarConfig const& c
) {
    SlotHistory& history =
        gHistory[static_cast<std::size_t>(slotIndex)];

    const int damage = stack.getDamageValue();
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
        enchantLevel(Enchant::Type::Mending, stack) > 0
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
    Player& player,
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
        auto const& stack =
            player.getArmor(
                kArmorSlots[static_cast<std::size_t>(i)].slot
            );

        if (stack.isNull() || !stack.isDamageableItem()) {
            continue;
        }

        const int max = maxDamage(stack);

        if (max <= 0) {
            continue;
        }

        const int damage = std::clamp(
            static_cast<int>(stack.getDamageValue()),
            0,
            max
        );

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
                stack.getRawNameId(),
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
    feature::ArmorSnapshot snapshot;
    int armorSum = 0;

    std::array<ItemStack const*, 4> stacks{};
    std::array<std::string, 4> ids{};

    for (std::size_t i = 0; i < kArmorSlots.size(); ++i) {
        auto const& stack = player.getArmor(kArmorSlots[i].slot);

        stacks[i] = &stack;

        feature::ArmorPiece& piece = snapshot.pieces[i];

        piece.equipped = !stack.isNull();
        piece.itemId = piece.equipped
            ? stack.getRawNameId()
            : std::string{};

        ids[i] = piece.itemId;
        piece.armorPoints = piece.equipped ? armorValue(stack) : 0;

        piece.damage = piece.equipped
            ? static_cast<int>(stack.getDamageValue())
            : 0;

        piece.maxDamage = piece.equipped ? maxDamage(stack) : 0;

        piece.mending =
            piece.equipped &&
            enchantLevel(Enchant::Type::Mending, stack) > 0;

        piece.thorns =
            piece.equipped &&
            enchantLevel(Enchant::Type::Thorns, stack) > 0;

        piece.enchanted = piece.equipped && stack.isEnchanted();

        piece.hasCustomItemBar =
            piece.equipped &&
            c.toggleItemBar &&
            piece.itemId.find("elytra") != std::string::npos;

        piece.customItemBarPoints = 2;
        armorSum += piece.armorPoints;

        if (piece.equipped && piece.maxDamage > 0) {
            const float percent =
                feature::durabilityPercentRemaining(
                    piece.damage,
                    piece.maxDamage
                );

            if (c.toggleDurability) {
                notifyDurability(
                    client,
                    stack,
                    static_cast<int>(i),
                    piece.itemId,
                    percent,
                    c
                );
            } else {
                if (
                    gHistory[i].itemId == piece.itemId &&
                    gHistory[i].lastDamage >= 0 &&
                    piece.damage < gHistory[i].lastDamage &&
                    piece.mending
                ) {
                    gLastMendingFrame = gFrame;
                }

                gHistory[i].itemId = piece.itemId;
                gHistory[i].lastDamage = piece.damage;
                gHistory[i].lastPercent = percent;
            }
        } else {
            gHistory[i] = {};
        }
    }

    snapshot.reportedArmorValue =
        std::max(0, player.getArmorValue());

    snapshot.baseArmor =
        std::max(0, snapshot.reportedArmorValue - armorSum);

    auto points = feature::buildBarPointLayout(snapshot, c);
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

    const float baseX =
        (screenLeft + screenRight) * 0.5f -
        45.0f +
        static_cast<float>(c.armorBarOffsetX);

    const float baseY =
        screenBottom -
        static_cast<float>(kBaseYFromBottom) +
        static_cast<float>(c.armorBarOffsetY) -
        static_cast<float>(rows - 1) * 10.0f;

    bool anyProtection = false;

    int genericUnits = 0;
    int projectileUnits = 0;
    int blastUnits = 0;
    int fireUnits = 0;

    for (auto const* ptr : stacks) {
        if (!ptr || ptr->isNull()) {
            continue;
        }

        const int generic =
            enchantLevel(Enchant::Type::Protection, *ptr);

        const int projectile =
            enchantLevel(Enchant::Type::ProjectileProtection, *ptr);

        const int blast =
            enchantLevel(Enchant::Type::BlastProtection, *ptr);

        const int fire =
            enchantLevel(Enchant::Type::FireProtection, *ptr);

        genericUnits += generic + (generic > 0 ? 1 : 0);
        projectileUnits += projectile;
        blastUnits += blast;
        fireUnits += fire;

        anyProtection =
            anyProtection ||
            (generic + projectile + blast + fire > 0);
    }

    std::vector<std::uint32_t> unalignedColors;

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

    if (
        gLastHealth >= 0.0f &&
        static_cast<float>(player.getHealth()) < gLastHealth
    ) {
        gLastHurtFrame = gFrame;
    }

    gLastHealth = static_cast<float>(player.getHealth());

    for (int pointIndex = 0; pointIndex < drawPoints; ++pointIndex) {
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
            x + (secondHalf != c.toggleInverseSlot ? 5.0f : 1.0f);

        const float halfW = static_cast<float>(kHalfPointWidth);

        fill(ctx, x + 1.0f, y + 1.0f, 7.0f, 7.0f, 0xff20252au);
        outline(ctx, x, y, 9.0f, 9.0f, 0xff7a838bu, 0.9f, 1);

        if (pointIndex >= static_cast<int>(points.size())) {
            continue;
        }

        const feature::BarPoint& point =
            points[static_cast<std::size_t>(pointIndex)];

        if (point.empty) {
            continue;
        }

        std::uint32_t baseColor = 0xffaeb6bfu;
        ItemStack const* pieceStack = nullptr;

        if (point.pieceIndex >= 0 && point.pieceIndex < 4) {
            pieceStack =
                stacks[static_cast<std::size_t>(point.pieceIndex)];

            if (pieceStack && !pieceStack->isNull()) {
                if (c.toggleArmorTypes) {
                    baseColor = materialColor(
                        ids[static_cast<std::size_t>(point.pieceIndex)],
                        c.toggleVanillaTexture
                    );
                } else {
                    baseColor = 0xffaeb6bfu;
                }
            }
        }

        fill(ctx, halfX, y + 1.0f, halfW, 7.0f, baseColor);

        if (
            c.toggleEnchants &&
            c.effectType != "NONE" &&
            c.toggleAlignEnchantments &&
            c.toggleUniformColor &&
            anyProtection
        ) {
            const float pulse =
                protectionPulse(c.effectType, c.effectSpeed, gFrame);

            if (c.effectType == "OUTLINE") {
                outline(
                    ctx,
                    halfX,
                    y + 1.0f,
                    halfW,
                    7.0f,
                    c.uniformColorArgb,
                    pulse,
                    1
                );
            } else {
                fill(
                    ctx,
                    halfX,
                    y + 1.0f,
                    halfW,
                    7.0f,
                    c.uniformColorArgb,
                    pulse
                );
            }
        }

        if (pieceStack && !pieceStack->isNull()) {
            const bool protectedPiece = hasProtection(*pieceStack);

            if (
                c.toggleEnchants &&
                c.toggleAlignEnchantments &&
                !c.toggleUniformColor &&
                protectedPiece &&
                c.effectType != "NONE"
            ) {
                const std::uint32_t protectionArgb =
                    feature::protectionColor(
                        protectionKind(*pieceStack),
                        c
                    );

                const float pulse =
                    protectionPulse(c.effectType, c.effectSpeed, gFrame);

                if (c.effectType == "OUTLINE") {
                    outline(
                        ctx,
                        halfX,
                        y + 1.0f,
                        halfW,
                        7.0f,
                        protectionArgb,
                        pulse,
                        1
                    );
                } else {
                    fill(
                        ctx,
                        halfX,
                        y + 1.0f,
                        halfW,
                        7.0f,
                        protectionArgb,
                        pulse
                    );
                }
            }

            if (
                c.toggleThorns &&
                enchantLevel(Enchant::Type::Thorns, *pieceStack) > 0
            ) {
                float thornAlpha = 1.0f;
                std::uint32_t thornColor = 0xffffffffu;

                const int duration =
                    feature::thornsAnimationDurationTicks(c.effectSpeed);

                const auto elapsed = gFrame - gLastHurtFrame;

                if (
                    c.effectThorn == "ANIMATION" &&
                    gLastHurtFrame != 0 &&
                    elapsed < static_cast<std::uint64_t>(duration)
                ) {
                    thornColor =
                        elapsed < static_cast<std::uint64_t>(duration / 2)
                        ? 0xffff3b3bu
                        : 0xffffa0a0u;

                    thornAlpha =
                        0.45f +
                        0.55f *
                            static_cast<float>(
                                duration - static_cast<int>(elapsed)
                            ) /
                            static_cast<float>(duration);
                }

                outline(
                    ctx,
                    halfX,
                    y + 1.0f,
                    halfW,
                    7.0f,
                    thornColor,
                    thornAlpha,
                    1
                );
            }

            if (
                c.toggleMending &&
                enchantLevel(Enchant::Type::Mending, *pieceStack) > 0 &&
                (gFrame % 16) < 9
            ) {
                const float sparkleX = halfX + halfW * 0.5f;

                fill(
                    ctx,
                    sparkleX,
                    y - 1.0f,
                    1.0f,
                    3.0f,
                    0xffffffffu
                );

                fill(
                    ctx,
                    sparkleX - 1.0f,
                    y,
                    3.0f,
                    1.0f,
                    0xffffffffu
                );
            }

            if (
                c.toggleMending &&
                enchantLevel(Enchant::Type::Mending, *pieceStack) > 0 &&
                gLastMendingFrame != 0 &&
                gFrame - gLastMendingFrame < 12
            ) {
                const float flash =
                    (gFrame - gLastMendingFrame) % 6 < 3
                    ? 1.0f
                    : 0.35f;

                outline(
                    ctx,
                    halfX,
                    y + 1.0f,
                    halfW,
                    7.0f,
                    0xffffffffu,
                    flash,
                    1
                );
            }

            const int max = maxDamage(*pieceStack);

            if (
                c.toggleDurability &&
                c.toggleDurabilityNotifications &&
                c.toggleDurabilityVisualEffect &&
                max > 0 &&
                point.pieceIndex >= 0 &&
                point.pieceIndex < 4
            ) {
                const float percent =
                    feature::durabilityPercentRemaining(
                        static_cast<int>(pieceStack->getDamageValue()),
                        max
                    );

                const auto warning =
                    feature::durabilityWarning(percent, c);

                const auto lastNotice =
                    gHistory[
                        static_cast<std::size_t>(point.pieceIndex)
                    ].lastNotificationFrame;

                if (
                    warning.level != feature::DurabilityLevel::None &&
                    lastNotice != 0
                ) {
                    const auto elapsed = gFrame - lastNotice;
                    float warningAlpha = 0.0f;

                    if (c.toggleRepeatedDurabilityNotifications) {
                        warningAlpha =
                            0.45f +
                            0.45f *
                                (
                                    0.5f +
                                    0.5f *
                                        std::sin(
                                            static_cast<float>(gFrame) *
                                            0.22f
                                        )
                                );
                    } else if (elapsed < 40) {
                        warningAlpha =
                            1.0f -
                            static_cast<float>(elapsed) / 40.0f;
                    }

                    if (warningAlpha > 0.0f) {
                        outline(
                            ctx,
                            halfX,
                            y + 1.0f,
                            halfW,
                            7.0f,
                            warning.colorArgb,
                            warningAlpha,
                            1
                        );
                    }
                }
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
                x + (secondHalf != c.toggleInverseSlot ? 5.0f : 1.0f);
            const float w = static_cast<float>(kHalfPointWidth);

            const auto color =
                unalignedColors[static_cast<std::size_t>(pointIndex)];

            const float pulse =
                protectionPulse(c.effectType, c.effectSpeed, gFrame);

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
                : baseX + 90.0f + shift;

            const float y = baseY + 1.0f;

            fill(ctx, x, y, 3.0f, 3.0f, 0xffffd34eu);
            fill(ctx, x, y + 4.0f, 3.0f, 3.0f, 0xffffd34eu);
        }
    }

    renderDurabilityHud(ctx, screen, player, c);
}

} // namespace

bool installArmorHudRenderer(config::ArmorBarConfig& config) {
    gConfig = &config;

    gHudListener =
        ll::event::EventBus::getInstance()
            .emplaceListener<ll::event::AfterUIRenderEvent>(
                [](ll::event::AfterUIRenderEvent& event) {
                    if (gConfig == nullptr) {
                        return;
                    }

                    ++gFrame;

                    auto& context = event.uiRenderContext();
                    auto& client = context.mClient;
                    auto* player = client.getLocalPlayer();

                    if (player == nullptr) {
                        return;
                    }

                    const auto screen =
                        context.getFullClippingRectangle();

                    renderArmorBar(
                        context,
                        screen,
                        *player,
                        client,
                        *gConfig
                    );
                }
            );

    return static_cast<bool>(gHudListener);
}

void uninstallArmorHudRenderer() {
    if (gHudListener) {
        ll::event::EventBus::getInstance().removeListener(
            gHudListener
        );

        gHudListener.reset();
    }

    gConfig = nullptr;
    gFrame = 0;
    gHistory = {};
    gLastHealth = -1.0f;
    gLastHurtFrame = 0;
    gLastMendingFrame = 0;
}

} // namespace dab::render
