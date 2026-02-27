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

        local llvm_prefix = os.getenv("LLVM_PREFIX") or "/opt/homebrew/opt/llvm@20"
        add_linkdirs(llvm_prefix .. "/lib/c++")
        add_ldflags("-Wl,-rpath," .. llvm_prefix .. "/lib/c++", {force = true})
    elseif is_plat("linux") then
        add_ldflags("-static-libstdc++", "-static-libgcc", {force = true})
    end

