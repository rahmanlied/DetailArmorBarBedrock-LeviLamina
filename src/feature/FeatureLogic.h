#pragma once

#include "../config/ArmorBarConfig.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dab::feature {

enum class ProtectionKind { Generic, Projectile, Blast, Fire };
enum class DurabilityLevel { None, Half, Quarter, Low, Critical };
enum class ArmorSlot { Head = 0, Chest = 1, Legs = 2, Feet = 3 };

struct ArmorPiece {
    std::string itemId;
    int armorPoints = 0;
    int damage = 0;
    int maxDamage = 0;
    bool equipped = false;
    bool mending = false;
    bool thorns = false;
    bool enchanted = false;
    std::string trimMaterial;
    // Set by a Bedrock adapter when a known special item has a custom armor-bar definition.
    bool hasCustomItemBar = false;
    int customItemBarPoints = 2;
};

struct ArmorSnapshot {
    std::array<ArmorPiece, 4> pieces{}; // Java source order: head, chest, legs, feet
    int baseArmor = 0;
    int reportedArmorValue = 0;
};

struct BarPoint {
    int pieceIndex = -1;    // -1 means vanilla/base/unattributed armor point
    int pointWithinPiece = 0;
    bool empty = false;
    bool specialItem = false;
};

struct DurabilityWarning {
    DurabilityLevel level = DurabilityLevel::None;
    float percentRemaining = 100.0f;
    std::uint32_t colorArgb = 0;
};

std::uint32_t protectionColor(ProtectionKind kind, const config::ArmorBarConfig& config);
float durabilityPercentRemaining(int currentDamage, int maxDamage);
DurabilityWarning durabilityWarning(float percentRemaining, const config::ArmorBarConfig& config);
int thornsAnimationDurationTicks(const std::string& speed);
std::vector<BarPoint> buildBarPointLayout(const ArmorSnapshot& snapshot, const config::ArmorBarConfig& config);
bool shouldHideArmorBar(int totalArmorPoints, const config::ArmorBarConfig& config);
int slotColumn(int slotIndex, bool inverseOrder, int slotsPerRow = 10);

} // namespace dab::feature
