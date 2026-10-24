#include "FeatureLogic.h"

#include <algorithm>
#include <cmath>

namespace dab::feature {

std::uint32_t protectionColor(ProtectionKind kind, const config::ArmorBarConfig& c) {
    if (c.toggleUniformColor) return c.uniformColorArgb;
    switch (kind) {
        case ProtectionKind::Generic: return c.protectionColorGenericArgb;
        case ProtectionKind::Projectile: return c.protectionColorProjectileArgb;
        case ProtectionKind::Blast: return c.protectionColorBlastArgb;
        case ProtectionKind::Fire: return c.protectionColorFireArgb;
    }
    return 0xFFFFFFFFu;
}

float durabilityPercentRemaining(int currentDamage, int maxDamage) {
    if (maxDamage <= 0) return -1.0f; // non-damageable item
    const float boundedDamage = static_cast<float>(std::clamp(currentDamage, 0, maxDamage));
    return std::clamp((1.0f - boundedDamage / static_cast<float>(maxDamage)) * 100.0f, 0.0f, 100.0f);
}

DurabilityWarning durabilityWarning(float percentRemaining, const config::ArmorBarConfig& c) {
    if (!std::isfinite(percentRemaining) || percentRemaining < 0.0f) return {};
    percentRemaining = std::clamp(percentRemaining, 0.0f, 100.0f);
    // Match upstream order: later/more severe enabled levels override less severe levels.
    DurabilityWarning result{DurabilityLevel::None, percentRemaining, 0};
    if (c.toggleThreshold50 && percentRemaining <= 50.0f)
        result = {DurabilityLevel::Half, percentRemaining, 0xFFFFFF00u};
    if (c.toggleThreshold25 && percentRemaining <= 25.0f)
        result = {DurabilityLevel::Quarter, percentRemaining, 0xFFFFA500u};
    if (c.toggleThreshold10 && percentRemaining <= 10.0f)
        result = {DurabilityLevel::Low, percentRemaining, 0xFFFF6400u};
    if (c.toggleThreshold5 && percentRemaining <= 5.0f)
        result = {DurabilityLevel::Critical, percentRemaining, 0xFFFF0000u};
    return result;
}

int thornsAnimationDurationTicks(const std::string& speed) {
    if (speed == "VERY_SLOW") return 20;
    if (speed == "SLOW") return 16;
    if (speed == "FAST") return 8;
    if (speed == "VERY_FAST") return 4;
    return 12; // NORMAL/default, matching Reconstructed's renderer.
}

std::vector<BarPoint> buildBarPointLayout(const ArmorSnapshot& s, const config::ArmorBarConfig& c) {
    std::vector<BarPoint> points;
    std::vector<BarPoint> special;
    int sumArmor = 0;

    for (int slot = 0; slot < static_cast<int>(s.pieces.size()); ++slot) {
        const ArmorPiece& piece = s.pieces[static_cast<std::size_t>(slot)];
        if (piece.equipped && piece.armorPoints > 0) {
            const int count = std::max(0, piece.armorPoints);
            sumArmor += count;
            for (int p = 0; p < count; ++p) points.push_back({slot, p, false, false});
        }
        if (c.toggleItemBar && piece.equipped && piece.hasCustomItemBar && piece.customItemBarPoints > 0) {
            std::vector<BarPoint> itemPoints;
            for (int p = 0; p < piece.customItemBarPoints; ++p)
                itemPoints.push_back({slot, p, false, true});
            if (c.toggleSortSpecialItem) {
                special.insert(special.end(), itemPoints.begin(), itemPoints.end());
            } else {
                if (points.size() % 2 == 1) points.push_back({-1, 0, true, false});
                points.insert(points.end(), itemPoints.begin(), itemPoints.end());
            }
        }
    }

    const int baseArmor = std::max(0, s.baseArmor);
    for (int i = 0; i < baseArmor; ++i) points.push_back({-1, i, false, false});
    sumArmor += baseArmor;
    const int unrepresented = std::max(0, s.reportedArmorValue - sumArmor);
    for (int i = 0; i < unrepresented; ++i) points.push_back({-1, i, false, false});

    if (c.toggleItemBar && c.toggleSortSpecialItem && !special.empty()) {
        if (points.size() % 2 == 1) points.push_back({-1, 0, true, false});
        points.insert(points.end(), special.begin(), special.end());
    }
    return points;
}

bool shouldHideArmorBar(int totalArmorPoints, const config::ArmorBarConfig& c) {
    return totalArmorPoints <= 0 && c.toggleHideBarWithoutArmor;
}

int slotColumn(int slotIndex, bool inverseOrder, int slotsPerRow) {
    if (slotsPerRow <= 0) return 0;
    const int normalized = std::clamp(slotIndex, 0, slotsPerRow - 1);
    return inverseOrder ? slotsPerRow - 1 - normalized : normalized;
}

} // namespace dab::feature
