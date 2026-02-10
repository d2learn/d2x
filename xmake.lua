add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_repositories("mcpplibs-index https://github.com/mcpplibs/mcpplibs-index.git")

add_requires("llmapi 0.0.2")
add_requires("ftxui 6.1.9")

target("d2x")
    set_kind("binary")

    add_files("src/main.cpp")
    -- add common module interface units
    add_files("src/**.cppm")
    add_packages("ftxui", "llmapi")
    set_policy("build.c++.modules", true)
    
    -- platform specific settings
    if is_plat("macosx") then
        -- macOS: use dynamic linking to avoid ABI conflicts with dependencies
        -- Dependencies (ftxui, llmapi) are built with system libc++
        -- Static linking causes memory allocator conflicts
        add_linkdirs("/opt/homebrew/Cellar/llvm@20/20.1.8/lib/c++")
        add_ldflags("-lc++experimental", {force = true})
        -- Add rpath to find homebrew libc++ at runtime
        add_ldflags("-Wl,-rpath,/opt/homebrew/opt/llvm@20/lib/c++", {force = true})
    else
        -- Linux/Windows: static link C++ runtime
        add_ldflags("-static-libstdc++", "-static-libgcc", {force = true})
    end

