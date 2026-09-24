#!/bin/bash
#
# build.sh - one-shot build script for ttf2ugui
#
#   ./build.sh          build only (configure if needed, no clean)
#   ./build.sh all      clean + configure + build + strip
#   ./build.sh clean    clean only (remove build/)
#   ./build.sh strip    strip only
#
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"

# ---------- helpers ----------

do_clean() {
    echo "=== clean ==="
    if [ -d "$BUILD" ]; then
        rm -rf "$BUILD"
        echo "removed $BUILD"
    else
        echo "$BUILD does not exist, nothing to clean"
    fi
}

do_configure() {
    echo "=== configure ==="
    mkdir -p "$BUILD"
    cd "$BUILD"

    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*)
            cmake .. -G "MinGW Makefiles"
            ;;
        *)
            cmake ..
            ;;
    esac
}

do_build() {
    echo "=== build ==="
    if [ ! -d "$BUILD" ] || [ ! -f "$BUILD/CMakeCache.txt" ]; then
        echo "build dir missing or not configured, running configure first"
        do_configure
    fi
    cd "$BUILD"
    cmake --build . --parallel
}

do_strip() {
    echo "=== strip ==="
    cd "$BUILD"
    cmake --build . --target stripall
}

# ---------- entry ----------

case "${1:-build}" in
    clean)
        do_clean
        ;;
    build|"")
        do_build
        ;;
    strip)
        do_strip
        ;;
    all)
        do_clean
        do_configure
        do_build
        do_strip
        echo "=== done ==="
        ls -lh "$BUILD"/ttf2ugui* 2>/dev/null || true
        echo "=== freetype linkage check ==="
        ldd "$BUILD/ttf2ugui.exe" 2>/dev/null | grep -i freetype || echo "OK: no external freetype dependency"
        ;;
    *)
        echo "usage: $0 [build|all|clean|strip]"
        exit 1
        ;;
esac