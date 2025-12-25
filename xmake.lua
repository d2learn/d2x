add_rules("mode.debug", "mode.release")

set_languages("c++23")

add_repositories("mcpplibs-index git@github.com:mcpplibs/mcpplibs-index.git")

add_requires("llmapi 0.0.1")
add_requires("ftxui 6.1.9", {configs = {modules = true}})

target("d2x")
    add_files("src/main.cpp")
    -- add common module interface units
    add_files("src/**.cppm")
    add_packages("ftxui", "llmapi")
    set_policy("build.c++.modules", true)