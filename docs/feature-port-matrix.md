# Feature port matrix: Detail Armor Bar Reconstructed → LeviLamina client

Source analyzed: uploaded `DetailArmorBarReconstructed-main.zip` plus older `DetailArmorBar-1.21-fabric-v3.zip`. The Reconstructed fork is authoritative for the newer features.

Legend: **Implemented (experimental)** means a native C++ code path exists but still needs a real client build/runtime test. **Partial** means the behavior is simplified. **Not implemented** means it is a known gap.

## Armor bar / rendering

| Java behavior | State | Notes |
|---|---|---|
| Per-piece armor points and total armor | Implemented (experimental) | Reads armor items and `Player::getArmorValue()`; needs runtime verification |
| Material armor appearance | Partial | Material identifier fallback colors; original texture/atlas renderer is missing |
| Empty bar | Implemented (experimental) | Draws empty pips when configured and visible |
| Hide without armor | Implemented (experimental) | Uses computed bar point count |
| X/Y offset | Implemented (experimental) | Applied to screen-space point layout; GUI scale alignment still needs testing |
| Inverse slot order | Implemented (experimental) | Mirrors point columns and half-points; must be visually verified |
| Stack armor bars above 20 points | Implemented (experimental) | Draws multiple rows; no-stack overflow is approximated with marker pips |
| Inverse overflow icon | Partial | Moves overflow markers to the alternate side |
| Special item bars / Elytra | Partial | Elytra gets two points; no custom item resource JSON/API yet |
| Sort special items | Implemented at layout-model level | Actual item mapping only supports built-in Elytra at present |
| Vanilla texture toggle | Partial | Toggles monochrome fallback color vs material fallback color; not the original sprite atlas |
| Compatibility with other HUD mods | Not implemented | Java mod-specific compatibility has no direct Bedrock equivalent |

## Enchantment effects

| Java behavior | State | Notes |
|---|---|---|
| Protection, Projectile Protection, Blast Protection, Fire Protection detection | Implemented (experimental) | Uses LeviLamina `EnchantUtils`; must validate enum/API on pinned version |
| Per-piece protection color | Implemented (experimental) | Color priority matches Java's generic → projectile → blast → fire order |
| Uniform color override | Implemented (experimental) | Applied across bar when any protection enchantment is present in aligned mode |
| NONE / AURA / OUTLINE / STATIC | Partial | Native fill/outline/pulse; not pixel/frame-identical to Java effect textures |
| Animation speed selection | Partial | Reuses the upstream tick duration mapping; aura pulse simplified |
| `toggleAlignEnchantments=true` | Implemented (experimental) | Colors per armor point's source piece |
| `toggleAlignEnchantments=false` | Partial | Aggregate level units map onto the first available HUD points; Java's exact split/consume algorithm is not fully reproduced |
| Thorns overlay | Partial | Reads Thorns, draws per-piece border and a health-drop flash; event trigger differs from Java's damage handler |
| `effectThorn` STATIC / ANIMATION | Partial | Static/animated border colors; timing simplified |
| Mending sparkle/flash | Partial | Reads Mending and checks durability decrease; timing simplified |
| Armor trim overlay | Not implemented | No Bedrock trim-data reader or trim texture pipeline yet |
| Dynamic custom/modded armor bars | Not implemented | Upstream API/resource loading not ported |

## Durability

| Java behavior | State | Notes |
|---|---|---|
| Durability percent | Implemented (experimental) | `1 - damage/maxDamage` |
| Durability HUD: four slots and percentages | Partial | Draws tiny numeric glyphs and progress meters, not actual item icons |
| Durability HUD position/scale/offsets | Implemented (experimental) | Four corners + X/Y offsets + scale; screen layout needs game testing |
| 50 / 25 / 10 / 5% thresholds | Implemented (experimental) | Uses configured toggles; threshold crossing queue not yet identical to Java |
| Toast and sound toggles | Partial | Popup/sound are called from client code, but in-game behavior is unverified |
| Repeat warnings | Partial | Cooldown-based repeat behavior, not same queue/tick semantics as Java |
| Visual warning outline | Implemented (experimental) | Outlines affected armor pips with warning colors |
| Inventory per-item overlay | Not implemented | Requires hooking the Bedrock inventory item's own draw/slot coordinates |

## Config / build

| Java behavior | State | Notes |
|---|---|---|
| Configuration defaults and option names | Implemented | `ArmorBarConfig` persists upstream option names to JSON |
| In-game config categories | Implemented in source, unverified | Visuals / Durability / Layout / Effects & Colors |
| Keybind to open menu | Partial | F8 hardcoded; no remappable key setting yet |
| Windows x64 AppVeyor build | Configured, not verified | Requires AppVeyor to run pinned LeviLamina build |
| Exact Bedrock 1.26.51.1 runtime validation | Not done | Must test the generated DLL in-game |
