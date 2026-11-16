-- Prints every target of the project as JSON: kind, add_deps and source files. Used by check_module_boundaries.py.
-- Run from the repository root, once configured: `xmake l scripts/xmake/dump_targets.lua`.
import("core.project.config")
import("core.project.project")
import("core.base.json")
function main()
    config.load()
    project.load_targets()
    local out = {}
    for name, target in pairs(project.targets()) do
        out[name] = {deps = table.wrap(target:get("deps")), files = target:sourcefiles(), kind = target:kind(), scriptdir = target:scriptdir()}
    end
    print(json.encode(out))
end
