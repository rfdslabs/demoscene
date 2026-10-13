#pragma once
// 8-bit indexed framebuffer — the authentic old-school representation.
// Each pixel is a palette index; the palette is 256 ARGB entries. Effects like
// plasma and copper bars are cheap because they write indices and animate the
// palette (palette cycling) instead of touching every pixel every frame.
namespace ti {
  constexpr int W = 320;
  constexpr int H = 200;

  struct Surface {
    unsigned char idx[W * H];
    unsigned int  pal[256];            // 0xAARRGGBB

    void clear(unsigned char c);
    void put(int x, int y, unsigned char c);        // bounds-checked
    unsigned char get(int x, int y) const;          // 0 if out of bounds
    void expand(unsigned int* argb_out) const;      // W*H ARGB for presentation
    void set_pal(int i, unsigned char r, unsigned char g, unsigned char b);
    void cycle_pal(int first, int count, int by);   // rotate a palette range
  };
}
