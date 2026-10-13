#pragma once
// Embedded 8x8 pixel font (procedural data, no asset file). Covers ASCII 32..90
// (space, punctuation, digits, A-Z). Lowercase maps to uppercase. Unknown -> space.
// Each glyph is 8 bytes, one per row (top to bottom); bit 7 is the leftmost pixel.
namespace ti {
  struct Surface;
  namespace font {
    constexpr int GW = 8;
    constexpr int GH = 8;
    const unsigned char* glyph(char c);            // 8 bytes
    bool glyph_pixel(char c, int gx, int gy);      // gx 0..7 L->R, gy 0..7 top->bottom
    int  text_width(const char* s);                // GW * length
    void draw(Surface& s, int x, int y, const char* str, unsigned char color);
  }
}
