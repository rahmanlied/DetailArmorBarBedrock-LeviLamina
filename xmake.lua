set_project("DetailArmorBarBedrock")
set_version("0.2.0-dev")
set_license("MIT")
set_languages("cxx20")
set_warnings("allextra")
set_runtimes("MD")

add_rules("mode.debug", "mode.release")
add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
    set_description("This project is a client-only LeviLamina mod")
option_end()

add_requires("levilamina v26.51.7", {configs = {target_type = get_config("target_type")}})
add_requires("levibuildscript")
add_requires("nlohmann_json")

if is_plat("windows") then
    set_toolchains("clang-cl")
    add_cxflags("/EHsc", "/utf-8", {tools = {"clang_cl"}})
    add_defines("NOMINMAX", "UNICODE", "_UNICODE")
end

-- Native client mod target. LeviLamina's build script generates the mod package
-- from manifest.json and the linked DLL.
target("DetailArmorBarBedrock")
    set_kind("shared")
    set_languages("cxx20")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_files("src/DetailArmorBarBedrock.cpp", "src/config/ArmorBarConfig.cpp", "src/feature/FeatureLogic.cpp", "src/ui/SettingsMenu.cpp", "src/render/ArmorHudRenderer.cpp")
    add_includedirs("src")
    add_packages("levilamina", "nlohmann_json")
    add_defines("NOMINMAX", "UNICODE", "_UNICODE")
target_end()

-- Pure logic tests: no Bedrock runtime required.
target("FeatureLogicTests")
    set_kind("binary")
    set_languages("cxx20")
    add_files("tests/feature_logic_tests.cpp", "src/feature/FeatureLogic.cpp")
    add_includedirs("src")
target_end()
