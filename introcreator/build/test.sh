#!/bin/bash
# Headless test build: engine + effects + testable panel logic, no Cocoa.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/out

# Collect source files that currently exist (tolerate empty phases).
SRC=$(find engine effects -name '*.cpp' 2>/dev/null || true)
# Panel logic that is host-compilable (no .mm) is added as those files land.
for f in panel/introspec.cpp panel/bake.cpp; do
  [ -f "$f" ] && SRC="$SRC $f"
done

clang++ -std=c++20 -O1 -g -Iengine -Ipanel -Itests \
  tests/*.cpp $SRC -o build/out/tests
./build/out/tests
