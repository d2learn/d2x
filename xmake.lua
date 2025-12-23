add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_requires("ftxui 6.1.9", {configs = {modules = true}})

target("d2x")
    add_files("src/main.cpp")
    -- add common module interface units
    add_files("src/**.cppm")
        --[[ platform-specific module units
        if is_plat("windows") then
            add_files("src/platform/windows.cppm")
        elseif is_plat("linux") then
            add_files("src/platform/linux.cppm")
        elseif is_plat("macosx") then
            add_files("src/platform/macos.cppm")
        end
        --]]
    add_packages("ftxui")
    set_policy("build.c++.modules", true)