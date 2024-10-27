add_rules("mode.debug", "mode.release")

set_languages("c++11")

target("cbricks")
    set_kind("binary")
    add_files("*.cpp")
    add_files("base/*.cpp")
    add_files("trace/*.cpp")
    add_files("log/*.cpp")
    add_files("io/*.cpp")
    add_files("server/*.cpp")
    add_files("sync/*.cpp")
    add_files("pool/*.cpp")
    add_files("memory/*.cpp")
    add_files("datastruct/*.cpp")
    -- add_files("mysql/*.cpp")

