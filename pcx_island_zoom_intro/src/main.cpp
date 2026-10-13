#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>

namespace {
constexpr int kBaseW = 640;
constexpr int kBaseH = 400;
constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;

using Map = std::vector<uint8_t>;

float Hash2(int x, int y, int seed) {
  uint32_t h = static_cast<uint32_t>(x * 374761393 + y * 668265263 + seed * 982451653);
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= (h >> 16);
  return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
}

void PutPixel(Map& m, int x, int y, uint8_t c) {
  if (x < 0 || y < 0 || x >= kBaseW || y >= kBaseH) return;
  m[static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x)] = c;
}

void Fill(Map& m, uint8_t c) { std::fill(m.begin(), m.end(), c); }

void DrawFilledCircle(Map& m, int cx, int cy, int r, uint8_t c) {
  const int r2 = r * r;
  for (int y = -r; y <= r; ++y) {
    for (int x = -r; x <= r; ++x) {
      if (x * x + y * y <= r2) PutPixel(m, cx + x, cy + y, c);
    }
  }
}

void DrawLine(Map& m, int x0, int y0, int x1, int y1, uint8_t c) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    PutPixel(m, x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void DrawPalm(Map& m, int x, int y, int trunkH) {
  for (int i = 0; i < trunkH; ++i) {
    PutPixel(m, x + (i / 5), y - i, 9);
    if ((i % 4) == 0) PutPixel(m, x + (i / 5) + 1, y - i, 10);
  }
  const int topX = x + trunkH / 5;
  const int topY = y - trunkH;
  DrawFilledCircle(m, topX - 2, topY + 2, 2, 10);
  DrawFilledCircle(m, topX + 2, topY + 2, 2, 10);
  DrawFilledCircle(m, topX, topY + 3, 2, 10);

  DrawLine(m, topX, topY, topX - 14, topY - 6, 11);
  DrawLine(m, topX, topY, topX - 16, topY + 1, 11);
  DrawLine(m, topX, topY, topX + 15, topY - 5, 11);
  DrawLine(m, topX, topY, topX + 16, topY + 2, 11);
  DrawLine(m, topX, topY, topX - 6, topY - 11, 11);
  DrawLine(m, topX, topY, topX + 7, topY - 10, 11);
}

void GenerateIsland(Map& out, int seed, int palmX, int palmY) {
  Fill(out, 0);

  const float sx = static_cast<float>(kBaseW) / 320.0f;
  const float sy = static_cast<float>(kBaseH) / 200.0f;
  const float cx = kBaseW * 0.5f + (Hash2(seed, 7, seed) - 0.5f) * 14.0f * sx;
  const float cy = kBaseH * 0.53f + (Hash2(seed, 17, seed) - 0.5f) * 10.0f * sy;
  const float rx = 88.0f * sx + (Hash2(seed, 31, seed) - 0.5f) * 12.0f * sx;
  const float ry = 52.0f * sy + (Hash2(seed, 47, seed) - 0.5f) * 10.0f * sy;

  for (int y = 0; y < kBaseH; ++y) {
    for (int x = 0; x < kBaseW; ++x) {
      const float nx = (x - cx) / rx;
      const float ny = (y - cy) / ry;
      const float d = nx * nx + ny * ny;
      uint8_t c = 0;
      if (d < 0.82f) {
        c = 6;  // grass
      } else if (d < 1.02f) {
        c = 4;  // sand
      } else if (d < 1.10f) {
        c = 3;  // bright shoreline ring
      } else {
        c = (Hash2(x, y, seed) > 0.5f) ? 0 : 1;  // water dither
      }

      // add oldschool noisy shading
      if (c == 6 && Hash2(x * 2, y * 3, seed + 9) > 0.82f) c = 7;
      if (c == 4 && Hash2(x * 5, y * 2, seed + 12) > 0.75f) c = 5;
      out[static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x)] = c;
    }
  }

  // rocks
  for (int i = 0; i < 8; ++i) {
    const int rxp = static_cast<int>(cx + (Hash2(i, seed, 77) - 0.5f) * 90.0f * sx);
    const int ryp = static_cast<int>(cy + (Hash2(i, seed, 101) - 0.5f) * 46.0f * sy);
    DrawFilledCircle(out, rxp, ryp, static_cast<int>((2 + (i % 3)) * ((sx + sy) * 0.5f)), 8);
  }

  // focal palm and additional palms
  DrawPalm(out, palmX, palmY, static_cast<int>(27 * sy));
  DrawPalm(out, palmX - static_cast<int>(32 * sx), palmY + static_cast<int>(7 * sy), static_cast<int>(20 * sy));
  DrawPalm(out, palmX + static_cast<int>(36 * sx), palmY + static_cast<int>(5 * sy), static_cast<int>(21 * sy));

  // little shoreline highlight
  for (int y = 0; y < kBaseH; ++y) {
    for (int x = 0; x < kBaseW; ++x) {
      uint8_t& px = out[static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x)];
      if (px == 4 && Hash2(x + seed, y - seed, 333) > 0.85f) px = 3;
      if ((px == 0 || px == 1) && Hash2(x * 7, y * 5, 444 + seed) > 0.985f) px = 2;
    }
  }
}

void BlitZoomed(const Map& src, std::vector<uint8_t>& dst, float centerX, float centerY, float zoom) {
  for (int y = 0; y < kBaseH; ++y) {
    for (int x = 0; x < kBaseW; ++x) {
      const float sx = centerX + (static_cast<float>(x) - kBaseW * 0.5f) / zoom;
      const float sy = centerY + (static_cast<float>(y) - kBaseH * 0.5f) / zoom;
      int ix = static_cast<int>(std::floor(sx));
      int iy = static_cast<int>(std::floor(sy));
      if (ix < 0 || iy < 0 || ix >= kBaseW || iy >= kBaseH) {
        dst[static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x)] = 0;
      } else {
        dst[static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x)] =
            src[static_cast<size_t>(iy) * kBaseW + static_cast<size_t>(ix)];
      }
    }
  }
}

// Optional: DEMO_SHOT=<seconds>:<file.bmp> saves one frame and exits (used for README screenshots).
void ParseShotEnv(float& at, std::string& path) {
  const char* env = std::getenv("DEMO_SHOT");
  if (!env) return;
  const std::string s(env);
  const size_t colon = s.find(':');
  if (colon == std::string::npos) return;
  at = std::strtof(s.substr(0, colon).c_str(), nullptr);
  path = s.substr(colon + 1);
}

void SaveFrameRenderer(SDL_Renderer* renderer, int w, int h, const std::string& path) {
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!s) return;
  SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
  SDL_SaveBMP(s, path.c_str());
  SDL_FreeSurface(s);
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("pcx_island_zoom_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWindowW, kWindowH, SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_Texture* tex =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, kBaseW, kBaseH);
  if (!tex) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // 16-ish color PCX-like palette.
  const std::array<SDL_Color, 16> pal = {{
      {8, 40, 78, 255},    // 0 deep water
      {12, 56, 103, 255},  // 1 water
      {34, 92, 140, 255},  // 2 foam
      {223, 208, 145, 255},// 3 bright sand
      {199, 180, 120, 255},// 4 sand
      {171, 152, 95, 255}, // 5 dark sand
      {66, 137, 76, 255},  // 6 grass
      {54, 118, 65, 255},  // 7 dark grass
      {96, 100, 94, 255},  // 8 rocks
      {115, 78, 43, 255},  // 9 trunk
      {145, 101, 58, 255}, // 10 trunk light
      {44, 124, 57, 255},  // 11 palm leaf
      {30, 82, 44, 255},   // 12 shadow
      {180, 164, 108, 255},// 13 optional
      {255, 255, 255, 255},
      {0, 0, 0, 255},
  }};

  Map mapA(static_cast<size_t>(kBaseW) * kBaseH);
  Map mapB(static_cast<size_t>(kBaseW) * kBaseH);
  std::vector<uint8_t> frameIdx(static_cast<size_t>(kBaseW) * kBaseH);
  std::vector<uint32_t> frameRGBA(static_cast<size_t>(kBaseW) * kBaseH);
  SDL_PixelFormat* rgbaFmt = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
  if (!rgbaFmt) {
    std::fprintf(stderr, "SDL_AllocFormat failed: %s\n", SDL_GetError());
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  const float sx = static_cast<float>(kBaseW) / 320.0f;
  const float sy = static_cast<float>(kBaseH) / 200.0f;
  const int palmAX = static_cast<int>(168 * sx);
  const int palmAY = static_cast<int>(112 * sy);
  const int palmBX = static_cast<int>(154 * sx);
  const int palmBY = static_cast<int>(108 * sy);
  GenerateIsland(mapA, 1337, palmAX, palmAY);
  GenerateIsland(mapB, 2025, palmBX, palmBY);

  bool running = true;
  bool fullscreen = false;
  bool useA = true;
  float zoom = 1.0f;
  Uint64 lastTicks = SDL_GetPerformanceCounter();

  float shotAt = -1.0f;
  std::string shotPath;
  ParseShotEnv(shotAt, shotPath);
  const Uint64 shotStart = SDL_GetPerformanceCounter();
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
        if (e.key.keysym.sym == SDLK_f) {
          fullscreen = !fullscreen;
          SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }
      }
    }

    Uint64 nowTicks = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    float dt = static_cast<float>((nowTicks - lastTicks) / freq);
    lastTicks = nowTicks;
    dt = std::min(dt, 0.04f);

    zoom *= std::pow(2.0f, dt * 1.45f);
    if (zoom > 30.0f) {
      zoom = 1.0f;
      useA = !useA;
    }

    const Map& src = useA ? mapA : mapB;
    const float cx = static_cast<float>(useA ? palmAX : palmBX);
    const float cy = static_cast<float>(useA ? palmAY - 25 : palmBY - 25);
    BlitZoomed(src, frameIdx, cx, cy, zoom);

    // conversion palette index -> rgba with tiny temporal dither.
    for (int y = 0; y < kBaseH; ++y) {
      for (int x = 0; x < kBaseW; ++x) {
        const size_t id = static_cast<size_t>(y) * kBaseW + static_cast<size_t>(x);
        uint8_t ci = frameIdx[id];
        if ((ci == 0 || ci == 1 || ci == 6) && ((x + y + static_cast<int>(zoom * 3.0f)) & 7) == 0) {
          ci = static_cast<uint8_t>(std::min(15, ci + 1));
        }
        const SDL_Color c = pal[ci & 15];
        frameRGBA[id] = SDL_MapRGBA(rgbaFmt, c.r, c.g, c.b, c.a);
      }
    }

    SDL_UpdateTexture(tex, nullptr, frameRGBA.data(), kBaseW * static_cast<int>(sizeof(uint32_t)));

    int winW = 0;
    int winH = 0;
    SDL_GetRendererOutputSize(renderer, &winW, &winH);
    const float scale = std::min(static_cast<float>(winW) / kBaseW, static_cast<float>(winH) / kBaseH);
    const int outW = static_cast<int>(kBaseW * scale);
    const int outH = static_cast<int>(kBaseH * scale);
    SDL_Rect dst{(winW - outW) / 2, (winH - outH) / 2, outW, outH};

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, tex, nullptr, &dst);

    // subtle old-monitor scanline overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 36);
    for (int y = dst.y; y < dst.y + dst.h; y += 3) {
      SDL_RenderDrawLine(renderer, dst.x, y, dst.x + dst.w, y);
    }

    if (shotAt >= 0.0f && static_cast<float>((nowTicks - shotStart) / freq) >= shotAt) {
      SaveFrameRenderer(renderer, winW, winH, shotPath);
      running = false;
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(tex);
  SDL_FreeFormat(rgbaFmt);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
