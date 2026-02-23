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
        local libcxx_dir = llvm_prefix .. "/lib/c++"
        -- Static link LLVM libc++ so binary has no *runtime* dependency on LLVM; use system libc++abi only.
        add_ldflags("-nostdlib++", {force = true})
        add_ldflags(libcxx_dir .. "/libc++.a", {force = true})
        -- Only needed if code uses std::experimental::* (e.g. experimental/filesystem). Remove if link succeeds.
        add_ldflags(libcxx_dir .. "/libc++experimental.a", {force = true})
        add_ldflags("-lc++abi", {force = true})  -- system /usr/lib/libc++abi.dylib
    elseif is_plat("linux") then
        add_ldflags("-static-libstdc++", "-static-libgcc", {force = true})
    end

