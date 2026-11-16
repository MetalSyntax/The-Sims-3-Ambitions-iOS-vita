#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "================================================================"
echo "  Building The Sims 3 Ambitions for PS Vita"
echo "================================================================"

if [ -z "$VITASDK" ]; then
    if [ -d "$HOME/vitasdk" ]; then
        export VITASDK="$HOME/vitasdk"
    elif [ -d "/usr/local/vitasdk" ]; then
        export VITASDK="/usr/local/vitasdk"
    fi
fi
export PATH="$VITASDK/bin:$PATH"

# Ensure vendored vitaGL is compiled
if [ ! -f "$PROJECT_DIR/vendor/vitaGL/libvitaGL.a" ]; then
    echo "[*] Compiling vendored vitaGL..."
    cd "$PROJECT_DIR/vendor/vitaGL"
    make SOFTFP_ABI=1 NO_DEBUG=1 HAVE_SHADER_CACHE=1 NO_SPLASHSCREEN=1 HAVE_GLSL_UBOS=1 SAMPLERS_SPEEDHACK=1 DRAW_SPEEDHACK=2 -j$(sysctl -n hw.ncpu)
    cd "$PROJECT_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(sysctl -n hw.ncpu)

echo "[+] Build complete!"
