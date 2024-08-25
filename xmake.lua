add_rules("mode.debug", "mode.release")

set_languages("c++11")


target("test")
    set_kind("binary")
    add_files("./*.cpp")
    add_files("./sync/*.cpp")
    add_files("./base/*.cpp")
