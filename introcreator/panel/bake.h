#pragma once
// Bake: turn an IntroSpec into a standalone, dependency-free native binary.
// Codegen emits a minimal generated/main.mm that embeds the timeline + text as
// plain constants and includes only the chosen effects; then clang compiles it
// with size flags. The generated folder is a real, readable project you keep —
// the "secrets of the code" artifact.
#include <string>
namespace ti {
  struct IntroSpec;

  // The text of the generated main.mm for this spec (pure, easy to test).
  std::string generate_main_source(const IntroSpec& spec);

  // Write the source into out_dir, compile to <out_dir>/intro, strip it, and
  // report the byte size. Returns false and fills *log with compiler output on
  // failure. Must be run from the project root (source paths are relative).
  bool bake(const IntroSpec& spec, const char* out_dir, std::string* log, long* size_bytes);
}
