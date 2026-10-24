#include "../src/feature/FeatureLogic.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace dab::feature;
using dab::config::ArmorBarConfig;

int main() {
    ArmorBarConfig c;
    assert(protectionColor(ProtectionKind::Generic, c) == 0x5099FFFFu);
    assert(protectionColor(ProtectionKind::Projectile, c) == 0x507033ADu);
    c.toggleUniformColor = true;
    c.uniformColorArgb = 0x8055AAFFu;
    assert(protectionColor(ProtectionKind::Fire, c) == 0x8055AAFFu);
    c.toggleUniformColor = false;

    assert(std::fabs(durabilityPercentRemaining(0, 100) - 100.0f) < 0.001f);
    assert(std::fabs(durabilityPercentRemaining(75, 100) - 25.0f) < 0.001f);
    assert(durabilityPercentRemaining(1, 0) < 0.0f);

    c.toggleThreshold50 = false;
    c.toggleThreshold25 = true;
    c.toggleThreshold10 = true;
    c.toggleThreshold5 = true;
    assert(durabilityWarning(80, c).level == DurabilityLevel::None);
    assert(durabilityWarning(24, c).level == DurabilityLevel::Quarter);
    assert(durabilityWarning(9, c).level == DurabilityLevel::Low);
    assert(durabilityWarning(4, c).level == DurabilityLevel::Critical);
    c.toggleThreshold5 = false;
    assert(durabilityWarning(4, c).level == DurabilityLevel::Low);

    assert(thornsAnimationDurationTicks("VERY_SLOW") == 20);
    assert(thornsAnimationDurationTicks("SLOW") == 16);
    assert(thornsAnimationDurationTicks("NORMAL") == 12);
    assert(thornsAnimationDurationTicks("FAST") == 8);
    assert(thornsAnimationDurationTicks("VERY_FAST") == 4);
    assert(slotColumn(0, false) == 0 && slotColumn(0, true) == 9);
    assert(slotColumn(9, false) == 9 && slotColumn(9, true) == 0);
    assert(shouldHideArmorBar(0, c));
    c.toggleHideBarWithoutArmor = false;
    assert(!shouldHideArmorBar(0, c));
    c.toggleHideBarWithoutArmor = true;

    ArmorSnapshot s;
    ArmorPiece helmet;
    helmet.itemId = "minecraft:iron_helmet";
    helmet.armorPoints = 2;
    helmet.equipped = true;
    s.pieces[0] = helmet;
    ArmorPiece elytra;
    elytra.itemId = "minecraft:elytra";
    elytra.equipped = true;
    elytra.hasCustomItemBar = true;
    elytra.customItemBarPoints = 2;
    s.pieces[1] = elytra;
    s.baseArmor = 1;
    s.reportedArmorValue = 3;
    c.toggleItemBar = true;
    c.toggleSortSpecialItem = true;
    auto layout = buildBarPointLayout(s, c);
    assert(layout.size() == 6); // 2 helmet + 1 base + 1 alignment pad + 2 special
    assert(layout[2].pieceIndex == -1 && !layout[2].empty);
    assert(layout[3].empty); // odd count is padded before the special item bar
    assert(layout[4].specialItem && layout[5].specialItem);

    c.toggleItemBar = false;
    auto noSpecialItems = buildBarPointLayout(s, c);
    assert(noSpecialItems.size() == 3); // armor points plus base armor, without Elytra's custom bar

    ArmorSnapshot overflow;
    overflow.reportedArmorValue = 24;
    overflow.pieces[0].equipped = true;
    overflow.pieces[0].armorPoints = 24;
    auto overflowPoints = buildBarPointLayout(overflow, c);
    assert(overflowPoints.size() == 24);

    std::cout << "FeatureLogicTests: all assertions passed\n";
    return 0;
}
