target("engine_input")
    set_kind("static")
    add_files("*.cpp", "../systems/input/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events", "engine_render_api", {public = true})
    if is_plat("windows", "mingw") then
        add_syslinks("user32", {public = true})
    end

-- Bridge between input and Wren: the `core/Input` module.
target("engine_input_scripting")
    set_kind("static")
    add_files("scripting/*.cpp")
    add_includedirs("..", {public = true})
    add_deps("engine_input", "engine_scripting")
