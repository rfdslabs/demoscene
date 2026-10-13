// Headless bake: load a .tro (or the built-in demo) and bake a tiny binary.
// Usage: bake_cli [path.tro]
#include "introspec.h"
#include "bake.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
  ti::IntroSpec spec = (argc > 1) ? ti::IntroSpec::load(argv[1]) : ti::IntroSpec::demo();
  if (spec.scenes.empty()) { std::printf("empty spec\n"); return 1; }
  std::string log; long bytes = 0;
  std::printf("baking '%s' (%zu scenes)...\n", spec.title.c_str(), spec.scenes.size());
  bool ok = ti::bake(spec, "generated", &log, &bytes);
  if (!ok) { std::printf("BAKE FAILED:\n%s\n", log.c_str()); return 1; }
  std::printf("OK -> generated/intro  (%ld bytes)\n", bytes);
  return 0;
}
