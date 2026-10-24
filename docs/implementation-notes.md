# Implementation notes — 0.2.0-dev

## Runtime flow

1. `DetailArmorBarBedrock::load()` loads/saves the upstream-named `ArmorBarConfig` JSON.
2. `enable()` subscribes to the local-client key event (F8 settings) and installs `AfterUIRenderEvent`.
3. The render listener accesses `MinecraftUIRenderContext::mClient`, obtains the local player, reads armor stacks for head/chest/legs/feet and draws a native rectangle-based armor HUD.
4. Each armor stack is queried for its item identifier, armor value, durability, and supported enchantment types via LeviLamina's item/enchantment API.
5. The renderer feeds a per-piece `ArmorSnapshot` to portable `FeatureLogic`, then applies colors/effects, durability HUD, notifications, Thorns outline, and Mending sparkle.

## Deliberately not represented as complete

The Java code uses texture atlas sprites, platform GUI mixins, screen slot render hooks and game-specific trim data. The native rectangle renderer cannot claim visual equivalence to that system. The following need distinct adapters rather than a fake approximation:

- ArmorTrim data decoding and colored trim texture drawing.
- Inventory item-slot render interception to draw each item overlay at its slot coordinates.
- Resource-pack JSON loading for `ArmorBarBuilder` / `ItemBarBuilder` definitions and custom modded armor.
- Exact aggregate enchantment split algorithm, effect phases and event timing.
- Exact durability notification queue/repeat semantics.

## Version-sensitive native APIs

The implementation is written against the documented LeviLamina client API shape for `AfterUIRenderEvent`, `MinecraftUIRenderContext`, `Player::getArmor`, `ItemStack`, and `EnchantUtils`. Since this workspace cannot install the pinned Windows Bedrock/LeviLamina SDK or execute the client, AppVeyor and an in-game run are still mandatory. An AppVeyor failure should be fixed against the reported `v26.51.1` header/signature; do not change the target version as a workaround without testing.
