#!/bin/bash
# Build headless helper tools (no window): bake_cli and the frame dumper.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/out
CORE="engine/surface.cpp engine/tables.cpp engine/font.cpp engine/registry.cpp effects/*.cpp panel/introspec.cpp"

clang++ -std=c++20 -O1 -Iengine -Ipanel \
  tools/bake_cli.cpp panel/bake.cpp $CORE -o build/out/bake_cli
clang++ -std=c++20 -O1 -Iengine -Ipanel \
  tools/dump_frames.cpp $CORE -o build/out/dump
echo "built build/out/bake_cli and build/out/dump"
