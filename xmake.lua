add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_repositories("mcpplibs-index https://github.com/mcpplibs/mcpplibs-index.git")

add_requires("llmapi 0.0.2")
add_requires("cmdline 0.0.2")
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
        set_toolchains("llvm")
        local llvm_prefix = os.getenv("LLVM_PREFIX")
        if llvm_prefix then -- if LLVM_PREFIX is set, we assume it's a recent version of LLVM that provides its own libc++ with C++23 support
            -- 静态链接 LLVM 自带的 libc++，避免依赖系统 libc++.dylib 中缺失的 C++23 符号
            -- (std::println 等 C++23 特性需要 macOS 15+ 的 libc++，静态链接后可在 macOS 11+ 运行)
            add_ldflags("-nostdlib++", {force = true})
            add_ldflags(llvm_prefix .. "/lib/libc++.a", {force = true})
            add_ldflags(llvm_prefix .. "/lib/libc++abi.a", {force = true})
        end
    elseif is_plat("linux") then
        add_ldflags("-static", {force = true})
    end

