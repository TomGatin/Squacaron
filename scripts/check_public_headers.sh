#!/usr/bin/env bash
# Compiles one translation unit that includes every installed engine header, with only the C++ standard library on
# the include path: asio, box2d, raylib and wren must stay inside the engine's .cpp files and private headers.
#
# Usage: scripts/check_public_headers.sh            (from the repository root; needs xmake and a C++23 compiler)
#        CXX=clang++ scripts/check_public_headers.sh
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Installs the headers of the engine_headers target (graphics on, so the render and audio headers are included).
(cd "$root/engine" && xmake f -P . --graphics=y -y >/dev/null && xmake install -P . -o "$work/install" engine_headers >/dev/null)

include="$work/install/include/squacaron"
if [ ! -d "$include" ]; then
    echo "error: no headers installed in $include" >&2
    exit 1
fi

tu="$work/all_headers.cpp"
count=0
while IFS= read -r header; do
    echo "#include \"${header#"$include"/}\"" >> "$tu"
    count=$((count + 1))
done < <(find "$include" -name '*.hpp' | sort)
echo "int main() { return 0; }" >> "$tu"

# A system-wide copy of a library (e.g. /usr/include/raylib.h) would let a leaked include compile: look for them too.
if grep -rnE '^\s*#\s*include\s*[<"](asio|box2d/|raylib|raymath|rlgl|wren)' "$include"; then
    echo "error: an installed header includes asio, box2d, raylib or wren (above)" >&2
    exit 1
fi

"${CXX:-c++}" -std=c++23 -fsyntax-only -I "$include" "$tu"
echo "ok: the ${count} installed headers compile without asio, box2d, raylib or wren"
