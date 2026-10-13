#!/bin/bash
# Build the native apps (dev flags). Produces:
#   build/out/tinyintro       - the creator panel (main app)
#   build/out/tinyintro-demo  - the standalone Phase-2 demo intro
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/out

SHARED="$(find engine effects -name '*.cpp') platform/mac_window.mm platform/mac_audio.mm panel/introspec.cpp"
[ -f panel/bake.cpp ] && SHARED="$SHARED panel/bake.cpp"

FLAGS="-std=c++20 -O2 -Iengine -Ipanel -Iplatform -fobjc-arc"
FRAMEWORKS="-framework Cocoa -framework QuartzCore -framework CoreAudio -framework AudioToolbox"

clang++ $FLAGS $SHARED panel/panel.cpp     -o build/out/tinyintro      $FRAMEWORKS
clang++ $FLAGS $SHARED platform/demo_main.mm -o build/out/tinyintro-demo $FRAMEWORKS
echo "built build/out/tinyintro and build/out/tinyintro-demo"
