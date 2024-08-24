add_rules("mode.debug", "mode.release")

set_languages("c++11")


target("cbricks")
    set_kind("binary")
    add_files("log/*.cpp")