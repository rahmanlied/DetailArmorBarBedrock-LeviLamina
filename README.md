# Detail Armor Bar Bedrock — LeviLamina client port

**Status: experimental client-render port (0.2.0-dev), not feature-complete.** This update adds an actual native HUD rendering path and reads the local player's armor/enchantment/durability data. It is still not equivalent to the Java Reconstructed mod: trim rendering, per-item inventory overlays, custom resource-pack bar definitions, and exact texture parity are not implemented yet, and the Bedrock native target has not been compiled/tested in this environment.

## Goal

Port [Detail Armor Bar Reconstructed](https://github.com/coredex-source/DetailArmorBarReconstructed) to Minecraft Bedrock **Windows x64 1.26.51.1** using **LeviLamina v26.51.1**, with the newer Reconstructed fork as the feature source of truth. The older [RedLime/DetailArmorBar](https://github.com/RedLime/DetailArmorBar) is used only as historical reference. This is a client HUD mod, not a dedicated-server mod.

## What this source now implements

| Feature path | Current implementation | Limits to know |
|---|---|---|
| Client UI render hook | `AfterUIRenderEvent` listener; gets local player via UI render context | Native ABI/API compatibility still needs AppVeyor and in-game validation |
| Armor item reading | Reads head/chest/legs/feet stacks, item IDs, item armor value, total armor points | Custom/modded armor may expose unusual stats not yet tested |
| Armor HUD | Draws half-point pips, material-colored fills, empty slots, X/Y offsets and row layout | Uses native rectangles/fallback colors, **not the original sprite renderer/textures** |
| Protection enchantments | Reads Protection, Projectile Protection, Blast Protection and Fire Protection per piece; configured colors and uniform color | The non-aligned aggregate mode is a native approximation, not pixel-identical Java behavior |
| Effect styles | Static fill, pulsing Aura, Outline | Timing/opacity has not been compared frame-by-frame with the Java renderer |
| Thorns | Reads Thorns on equipped items; highlights those pips and flashes after local health decreases | The source's exact damage event semantics are not reproduced yet |
| Mending | Reads Mending and draws animated sparkle; durability decreasing while Mending is present updates the flash timer | No exact match for Java's hit/tick event timing yet |
| Durability warnings | Tracks durability percent and configured thresholds; popup and UI sound are controlled by config | Toast timing/repeat queue is simplified compared with the Java handler |
| Durability HUD | Four equipment rows with progress bars and tiny numeric percentage glyphs; position, offsets and scale | Item icons are simplified markers rather than Bedrock item rendering |
| Special item | Elytra gets an approximate two-point item bar | Java's user-defined ItemBarBuilder/custom JSON API is not ported |
| In-game settings | F8 opens Visuals / Durability / Layout / Effects & Colors forms and writes `config.json` | Key is fixed, not remappable; form API must be confirmed in-game |

## Known gaps versus Java Reconstructed

These are not done and must not be treated as working features:

- **Armor trim overlay:** the Bedrock-side trim component/data adapter and colored trim texture renderer are not implemented.
- **Inventory overlay:** no per-inventory-item overlay hook has been implemented.
- **Original armor bar textures:** the upstream textures and example JSON are reference material only; the renderer currently uses native rectangles.
- **Custom armor bar resources/API:** upstream `ArmorBarBuilder`, `ItemBarBuilder`, resource-pack reload and dynamic custom armor definitions are not ported.
- **Compatibility mode for other Java HUD mods:** no Bedrock equivalent has been defined.
- **Full behavioral parity:** effect frames, enchantment-level aggregation, durability notification queues, trim colors and inventory behavior have not all been matched to the Java code.

The exact checklist is in [`docs/feature-port-matrix.md`](docs/feature-port-matrix.md). Don't publish this as a finished/1:1 port until each gap is implemented and verified.

## Build with AppVeyor

1. Create a GitHub repository and upload this folder's contents to its root.
2. Enable AppVeyor for the repository and push a commit.
3. AppVeyor installs XMake/LLVM, configures Windows x64 `target_type=client`, runs the pure `FeatureLogicTests`, and attempts to build/package the native DLL.
4. If the native build fails, use the actual AppVeyor log to correct any version-specific headers or method signatures. This environment has no installed LeviLamina client SDK and could not run the Windows build, so no successful DLL build is claimed.

```powershell
xmake f -y -p windows -a x64 -m release --target_type=client
xmake build FeatureLogicTests
xmake run FeatureLogicTests
xmake build DetailArmorBarBedrock
```

The dependency is pinned to `levilamina v26.51.1`. The package version target does not by itself prove compatibility with a Bedrock executable; test with the exact 1.26.51.1 client.

## Settings

The form menu is intended to open with **F8** while inside a world. Config is stored under the mod config directory as `config.json`. Color inputs accept 8 hexadecimal ARGB digits, such as `5099FFFF`. The key binding is hardcoded at present and cannot yet be remapped in the form.

## Assets and licensing

`resources/java-reference/` retains the upstream icon, armor bar/trim textures and sample JSON as **reference assets only**. These are not yet wired into a Bedrock-native texture pipeline. Preserve original attribution and MIT license terms; see [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

## Required release tests

- [ ] AppVeyor completes the native build and creates the package.
- [ ] Load the DLL in the exact Bedrock 1.26.51.1 + LeviLamina v26.51.1 client without exceptions.
- [ ] Test each armor piece, mixed material sets, custom/modded armor, Elytra, empty armor, >20 armor points, inverse slots, offsets and all GUI scales.
- [ ] Compare both enchantment-alignment modes and all protection types with the Java source.
- [ ] Verify Thorns/Mending timing and all durability thresholds, sound, toast, repeat and visual toggles.
- [ ] Implement and test trim colors, per-item inventory overlays, original textures and custom resource-pack definitions.
- [ ] Verify menu open/close/save/reload and all configuration options.
