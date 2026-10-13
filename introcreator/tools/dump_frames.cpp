// Headless frame dumper: renders the shared player at several timestamps to BMP
// files so effects can be eyeballed without a window. Not part of the app.
#include "introspec.h"
#include "surface.h"
#include "tables.h"
#include <cstdio>
#include <cstdlib>

static void write_bmp(const char* path, const unsigned int* argb, int w, int h) {
  FILE* f = fopen(path, "wb");
  if (!f) return;
  int row = (w * 3 + 3) & ~3;
  int size = 54 + row * h;
  unsigned char hdr[54] = {0};
  hdr[0]='B'; hdr[1]='M';
  *(int*)&hdr[2] = size;
  *(int*)&hdr[10] = 54;
  *(int*)&hdr[14] = 40;
  *(int*)&hdr[18] = w;
  *(int*)&hdr[22] = h;
  *(short*)&hdr[26] = 1;
  *(short*)&hdr[28] = 24;
  fwrite(hdr, 1, 54, f);
  unsigned char* line = (unsigned char*)calloc(row, 1);
  for (int y = h - 1; y >= 0; --y) {          // BMP is bottom-up
    for (int x = 0; x < w; ++x) {
      unsigned int p = argb[y * w + x];
      line[x*3+0] = p & 0xFF;                  // B
      line[x*3+1] = (p >> 8) & 0xFF;           // G
      line[x*3+2] = (p >> 16) & 0xFF;          // R
    }
    fwrite(line, 1, row, f);
  }
  free(line);
  fclose(f);
}

int main() {
  tt::init_tables();
  ti::IntroSpec spec = ti::IntroSpec::demo();
  static ti::Surface s;
  static unsigned int argb[ti::W * ti::H];
  // seconds = beats * 60 / bpm(125); pick a time inside each scene.
  double times[] = {11.0, 34.0, 19.0, 27.0, 50.0};
  const char* names[] = {"logo","vector3d","plasma","copperbars","tunnel"};
  for (int i = 0; i < 5; ++i) {
    ti::play(spec, s, times[i]);
    s.expand(argb);
    char path[128];
    std::snprintf(path, sizeof path, "build/out/frame_%d_%s.bmp", i, names[i]);
    write_bmp(path, argb, ti::W, ti::H);
    std::printf("wrote %s\n", path);
  }
  return 0;
}
