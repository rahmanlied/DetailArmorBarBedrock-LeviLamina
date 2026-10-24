#include "SettingsMenu.h"
#include "ll/api/form/CustomForm.h"
#include "ll/api/form/SimpleForm.h"
#include "mc/world/actor/player/Player.h"
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

namespace dab::ui {
namespace {
using ll::form::CustomForm;
using ll::form::CustomFormResult;
using ll::form::FormCancelReason;
using ll::form::SimpleForm;

bool toggle(CustomFormResult const& r, char const* key, bool fallback) {
    if (!r) return fallback;
    auto i = r->find(key);
    if (i == r->end()) return fallback;
    if (auto v = std::get_if<uint64>(&i->second)) return *v != 0;
    return fallback;
}
double number(CustomFormResult const& r, char const* key, double fallback) {
    if (!r) return fallback;
    auto i = r->find(key);
    if (i == r->end()) return fallback;
    if (auto v = std::get_if<double>(&i->second)) return *v;
    if (auto v = std::get_if<uint64>(&i->second)) return static_cast<double>(*v);
    return fallback;
}
void save(config::ArmorBarConfig const& c, std::filesystem::path const& p) { c.save(p); }
std::string colorText(std::uint32_t color) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << color;
    return out.str();
}
std::uint32_t parseColor(CustomFormResult const& r, char const* key, std::uint32_t fallback) {
    if (!r) return fallback;
    auto i = r->find(key);
    if (i == r->end()) return fallback;
    auto value = std::get_if<std::string>(&i->second);
    if (!value) return fallback;
    std::string raw = *value;
    if (!raw.empty() && raw.front() == '#') raw.erase(raw.begin());
    try {
        size_t used = 0;
        auto parsed = std::stoul(raw, &used, 16);
        if (used != raw.size() || parsed > 0xFFFFFFFFul) return fallback;
        return static_cast<std::uint32_t>(parsed);
    } catch (...) { return fallback; }
}
void visuals(Player&, config::ArmorBarConfig&, std::filesystem::path);
void durability(Player&, config::ArmorBarConfig&, std::filesystem::path);
void layout(Player&, config::ArmorBarConfig&, std::filesystem::path);
void effects(Player&, config::ArmorBarConfig&, std::filesystem::path);

void visuals(Player& p, config::ArmorBarConfig& c, std::filesystem::path path) {
    CustomForm f("Detail Armor Bar - Visuals");
    f.appendToggle("enchants", "Enchantment overlays", c.toggleEnchants)
     .appendToggle("types", "Armor material textures", c.toggleArmorTypes)
     .appendToggle("thorns", "Thorns overlay", c.toggleThorns)
     .appendToggle("mending", "Mending effect", c.toggleMending)
     .appendToggle("trims", "Armor trims", c.toggleArmorTrims)
     .appendToggle("inventory", "Inventory overlay", c.toggleInventoryOverlay)
     .appendToggle("vanilla", "Vanilla-style textures", c.toggleVanillaTexture)
     .appendToggle("empty", "Show empty armor slots", c.toggleEmptyBar)
     .appendToggle("itembar", "Special item bars (e.g. Elytra)", c.toggleItemBar)
     .setSubmitButton("Save");
    f.sendTo(p, [&c, path](Player&, CustomFormResult const& r, FormCancelReason) {
        if (!r) return;
        c.toggleEnchants=toggle(r,"enchants",c.toggleEnchants); c.toggleArmorTypes=toggle(r,"types",c.toggleArmorTypes);
        c.toggleThorns=toggle(r,"thorns",c.toggleThorns); c.toggleMending=toggle(r,"mending",c.toggleMending);
        c.toggleArmorTrims=toggle(r,"trims",c.toggleArmorTrims); c.toggleInventoryOverlay=toggle(r,"inventory",c.toggleInventoryOverlay);
        c.toggleVanillaTexture=toggle(r,"vanilla",c.toggleVanillaTexture); c.toggleEmptyBar=toggle(r,"empty",c.toggleEmptyBar);
        c.toggleItemBar=toggle(r,"itembar",c.toggleItemBar); save(c,path);
    });
}
void durability(Player& p, config::ArmorBarConfig& c, std::filesystem::path path) {
    const std::vector<std::string> positions{"TOP_LEFT","TOP_RIGHT","BOTTOM_LEFT","BOTTOM_RIGHT"};
    size_t pos = c.durabilityHudPosition=="TOP_LEFT" ? 0 : c.durabilityHudPosition=="TOP_RIGHT" ? 1 : c.durabilityHudPosition=="BOTTOM_RIGHT" ? 3 : 2;
    CustomForm f("Detail Armor Bar - Durability");
    f.appendToggle("enabled", "Enable durability features", c.toggleDurability)
     .appendToggle("overlay", "Durability HUD overlay", c.toggleDurabilityOverlay)
     .appendToggle("notifications", "Durability notifications", c.toggleDurabilityNotifications)
     .appendToggle("repeat", "Repeat notifications", c.toggleRepeatedDurabilityNotifications)
     .appendToggle("sound", "Notification sound", c.toggleDurabilitySoundNotification)
     .appendToggle("toast", "Toast notification", c.toggleDurabilityToastNotification)
     .appendToggle("visual", "Visual warning effect", c.toggleDurabilityVisualEffect)
     .appendToggle("t50", "Warn at 50%", c.toggleThreshold50)
     .appendToggle("t25", "Warn at 25%", c.toggleThreshold25)
     .appendToggle("t10", "Warn at 10%", c.toggleThreshold10)
     .appendToggle("t5", "Warn at 5%", c.toggleThreshold5)
     .appendSlider("scale", "HUD scale", 0.5, 3.0, 0.1, c.durabilityHudScale)
     .appendDropdown("position", "HUD position", positions, pos)
     .setSubmitButton("Save");
    f.sendTo(p, [&c,path,positions](Player&, CustomFormResult const& r, FormCancelReason) {
        if (!r) return;
        c.toggleDurability=toggle(r,"enabled",c.toggleDurability); c.toggleDurabilityOverlay=toggle(r,"overlay",c.toggleDurabilityOverlay);
        c.toggleDurabilityNotifications=toggle(r,"notifications",c.toggleDurabilityNotifications);
        c.toggleRepeatedDurabilityNotifications=toggle(r,"repeat",c.toggleRepeatedDurabilityNotifications);
        c.toggleDurabilitySoundNotification=toggle(r,"sound",c.toggleDurabilitySoundNotification);
        c.toggleDurabilityToastNotification=toggle(r,"toast",c.toggleDurabilityToastNotification);
        c.toggleDurabilityVisualEffect=toggle(r,"visual",c.toggleDurabilityVisualEffect);
        c.toggleThreshold50=toggle(r,"t50",c.toggleThreshold50); c.toggleThreshold25=toggle(r,"t25",c.toggleThreshold25);
        c.toggleThreshold10=toggle(r,"t10",c.toggleThreshold10); c.toggleThreshold5=toggle(r,"t5",c.toggleThreshold5);
        c.durabilityHudScale=static_cast<float>(std::clamp(number(r,"scale",c.durabilityHudScale),0.5,3.0));
        auto i=static_cast<size_t>(std::clamp(number(r,"position",2),0.0,3.0)); c.durabilityHudPosition=positions[i]; save(c,path);
    });
}
void layout(Player& p, config::ArmorBarConfig& c, std::filesystem::path path) {
    CustomForm f("Detail Armor Bar - Layout");
    f.appendToggle("hide", "Hide bar without armor", c.toggleHideBarWithoutArmor)
     .appendToggle("compat", "Compatibility with heart HUD mods", c.toggleCompatibleHeartMod)
     .appendToggle("inverse", "Reverse slot order", c.toggleInverseSlot)
     .appendToggle("overflow", "Reverse overflow icon", c.toggleInverseOverflowIcon)
     .appendToggle("stack", "Stack armor bars over 20 points", c.toggleStackArmorBars)
     .appendToggle("sort", "Sort special item bars", c.toggleSortSpecialItem)
     .appendToggle("align", "Align enchantment overlays", c.toggleAlignEnchantments)
     .appendSlider("x", "Armor bar X offset", -200, 200, 1, c.armorBarOffsetX)
     .appendSlider("y", "Armor bar Y offset", -200, 200, 1, c.armorBarOffsetY)
     .appendSlider("dx", "Durability HUD X offset", -200, 200, 1, c.durabilityHudOffsetX)
     .appendSlider("dy", "Durability HUD Y offset", -200, 200, 1, c.durabilityHudOffsetY)
     .setSubmitButton("Save");
    f.sendTo(p, [&c,path](Player&, CustomFormResult const& r, FormCancelReason) {
        if (!r) return;
        c.toggleHideBarWithoutArmor=toggle(r,"hide",c.toggleHideBarWithoutArmor); c.toggleCompatibleHeartMod=toggle(r,"compat",c.toggleCompatibleHeartMod);
        c.toggleInverseSlot=toggle(r,"inverse",c.toggleInverseSlot); c.toggleInverseOverflowIcon=toggle(r,"overflow",c.toggleInverseOverflowIcon);
        c.toggleStackArmorBars=toggle(r,"stack",c.toggleStackArmorBars); c.toggleSortSpecialItem=toggle(r,"sort",c.toggleSortSpecialItem);
        c.toggleAlignEnchantments=toggle(r,"align",c.toggleAlignEnchantments);
        c.armorBarOffsetX=static_cast<int>(number(r,"x",c.armorBarOffsetX)); c.armorBarOffsetY=static_cast<int>(number(r,"y",c.armorBarOffsetY));
        c.durabilityHudOffsetX=static_cast<int>(number(r,"dx",c.durabilityHudOffsetX)); c.durabilityHudOffsetY=static_cast<int>(number(r,"dy",c.durabilityHudOffsetY)); save(c,path);
    });
}
void effects(Player& p, config::ArmorBarConfig& c, std::filesystem::path path) {
    const std::vector<std::string> types{"NONE","AURA","OUTLINE","STATIC"};
    const std::vector<std::string> speeds{"VERY_SLOW","SLOW","NORMAL","FAST","VERY_FAST"};
    const std::vector<std::string> thornTypes{"STATIC","ANIMATION"};
    // Match the eight color presets offered by the Java OptionsScreen, plus custom ARGB.
    const std::vector<std::string> colors{"AQUA","PURPLE","YELLOW","ORANGE","WHITE","GREEN","BLUE","RED","CUSTOM"};
    const std::vector<std::uint32_t> presetArgb{
        0x5099FFFFu, 0x507033ADu, 0x50FFFF00u, 0x50D23800u,
        0x50FFFFFFu, 0x5000FF00u, 0x500000FFu, 0x50FF0000u
    };
    auto index=[](std::vector<std::string> const& v,std::string const& s){auto i=std::find(v.begin(),v.end(),s);return i==v.end()?size_t{0}:static_cast<size_t>(i-v.begin());};
    CustomForm f("Detail Armor Bar - Effects & Colors");
    f.appendDropdown("effect", "Effect style", types, index(types,c.effectType))
     .appendDropdown("speed", "Animation speed", speeds, index(speeds,c.effectSpeed))
     .appendDropdown("thorn", "Thorns effect", thornTypes, index(thornTypes,c.effectThorn))
     .appendToggle("uniform", "Use one color for all protection types", c.toggleUniformColor)
     .appendDropdown("color", "Uniform color preset", colors, index(colors,c.uniformColorType))
     .appendInput("uniformArgb", "Uniform ARGB color (8 hex digits)", "e.g. 5099FFFF", colorText(c.uniformColorArgb))
     .appendInput("genericArgb", "Protection ARGB color", "e.g. 5099FFFF", colorText(c.protectionColorGenericArgb))
     .appendInput("projectileArgb", "Projectile Protection ARGB", "e.g. 507033AD", colorText(c.protectionColorProjectileArgb))
     .appendInput("blastArgb", "Blast Protection ARGB", "e.g. 50FFFF00", colorText(c.protectionColorBlastArgb))
     .appendInput("fireArgb", "Fire Protection ARGB", "e.g. 50D23800", colorText(c.protectionColorFireArgb))
     .setSubmitButton("Save");
    f.sendTo(p, [&c,path,types,speeds,thornTypes,colors,presetArgb](Player&, CustomFormResult const& r, FormCancelReason) {
        if (!r) return;
        auto idx=[&r](char const* k,size_t n){return static_cast<size_t>(std::clamp(number(r,k,0),0.0,static_cast<double>(n-1)));};
        c.effectType=types[idx("effect",types.size())]; c.effectSpeed=speeds[idx("speed",speeds.size())];
        c.effectThorn=thornTypes[idx("thorn",thornTypes.size())]; c.toggleUniformColor=toggle(r,"uniform",c.toggleUniformColor);
        c.uniformColorType=colors[idx("color",colors.size())];
        c.uniformColorArgb=parseColor(r,"uniformArgb",c.uniformColorArgb);
        auto presetIt = std::find(colors.begin(), colors.end(), c.uniformColorType);
        if (presetIt != colors.end()) {
            const auto presetIndex = static_cast<std::size_t>(std::distance(colors.begin(), presetIt));
            if (presetIndex < presetArgb.size()) c.uniformColorArgb = presetArgb[presetIndex];
        }
        c.protectionColorGenericArgb=parseColor(r,"genericArgb",c.protectionColorGenericArgb);
        c.protectionColorProjectileArgb=parseColor(r,"projectileArgb",c.protectionColorProjectileArgb);
        c.protectionColorBlastArgb=parseColor(r,"blastArgb",c.protectionColorBlastArgb);
        c.protectionColorFireArgb=parseColor(r,"fireArgb",c.protectionColorFireArgb);
        save(c,path);
    });
}
}
void openSettingsMenu(Player& player, config::ArmorBarConfig& c, std::filesystem::path path) {
    SimpleForm menu("Detail Armor Bar", "Settings are saved to config.json.");
    menu.addButton("Visuals", [&c,path](Player& p){visuals(p,c,path);})
        .addButton("Durability", [&c,path](Player& p){durability(p,c,path);})
        .addButton("Layout", [&c,path](Player& p){layout(p,c,path);})
        .addButton("Effects & Colors", [&c,path](Player& p){effects(p,c,path);})
        .addButton("Close");
    menu.sendTo(player);
}
} // namespace dab::ui
