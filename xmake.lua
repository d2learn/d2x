set_languages("c++23")

target("d2x")
    add_files("src/main.cpp")
    add_files("src/**.mpp")
    set_policy("build.c++.modules", true)