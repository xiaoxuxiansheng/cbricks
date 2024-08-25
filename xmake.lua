add_rules("mode.debug", "mode.release")

set_languages("c++11")

target("cbricks")
    set_kind("binary")
    add_files("*.cpp")
    add_files("base/*.cpp")
    add_files("sync/*.cpp")

-- target("unitTest")
--     set_kind("binary")
--     set_default(false)
--     add_files("test/thread.test.cpp")
--     add_tests("GETID")
