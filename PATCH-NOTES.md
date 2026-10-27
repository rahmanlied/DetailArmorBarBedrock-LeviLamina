# HUD replacement, Java atlas, F8 and render-cost patch

This patch is based on the source ZIP provided in the conversation. It retains the C++23, LeviLamina 26.51.7, MemoryOperators, and AppVeyor cache configuration.

## Changes made

- Replaces the `HudArmorRenderer::render` call with a LeviLamina hook. The hook deliberately does not call the vanilla renderer and draws the mod bar in that renderer's own path, instead of painting a second bar from every `AfterUIRenderEvent` callback.
- Uses the Java `resources/java-reference/textures/armor_bar.png` atlas for the normal, half, empty, material, and special-item sprites. Icon sizing is 9x9 with an 8px stride, and material offsets follow the Java source. The vanilla texture variant adds the Java atlas row offset of 45 pixels.
- Adds `resources/DetailArmorBarBedrockTextures.zip` to the AppVeyor distribution, with `textures/armor_bar.png` at the archive root so `getZippedTexture("textures/armor_bar")` can resolve it.
- Reduces work in the HUD draw path: armor/enchantment/durability state is cached for eight HUD-hook frames, base sprite drawing is one image per full icon (only split-material icons need two), and protection/thorns/mending/durability overlays are grouped per icon rather than re-filling every half-point.
- Replaces the unreliable F8 key-event path with edge-detected Win32 F8 polling from `AfterUIRenderEvent`, and writes a log entry when F8 is detected or the local player is unavailable.
- Writes generated `manifest.json` as UTF-8 without a BOM to address BMCBL's `expected value at line 1 column 1` manifest warning.
- Keeps the AppVeyor cache paths that no longer invalidate XMake package/download caches just because `xmake.lua` changes.

## Known limits and validation status

- The Java atlas contains sprites for the Java armor types. Copper armor uses the neutral iron/silver sprite because the provided Java atlas has no copper sprite. Per-item leather dye colors, arbitrary resource-pack custom armor bars, full Java enchantment aura animation/tinting, and armor-trim sprites are not implemented by this patch.
- The F8 listener logs whether polling sees the key and submits the existing settings form. Actual form display must still be confirmed in the user's GDK client.
- This workspace does not contain the pinned Windows Bedrock/LeviLamina SDK/link environment, so the native DLL was not compiled here. The portable `FeatureLogicTests` were compiled and passed, but the complete AppVeyor build and in-game checks remain required before treating the patch as working.
