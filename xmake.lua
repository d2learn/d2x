add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_repositories("mcpplibs-index https://github.com/mcpplibs/mcpplibs-index.git")

add_requires("llmapi 0.0.2")
add_requires("cmdline 0.0.1")
add_requires("ftxui 6.1.9")

target("d2x")
    set_kind("binary")

    add_files("src/main.cpp")
    -- add common module interface units
    add_files("src/**.cppm")
    add_packages("ftxui", "llmapi", "cmdline")
    set_policy("build.c++.modules", true)
    
    -- platform specific settings
    if is_plat("macosx") then
        --set_toolchains("clang") -- build error
        set_toolchains("llvm")
    elseif is_plat("linux") then
        add_ldflags("-static-libstdc++", "-static-libgcc", {force = true})
    end