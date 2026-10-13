#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kSampleRate = 48000;
constexpr float kPi = 3.14159265359f;

struct AudioState {
  float t = 0.0f;
  float kickEnv = 0.0f;
  float nextStep = 0.0f;
  int step = 0;
  float bassPhase = 0.0f;
  float leadPhase = 0.0f;
  std::atomic<float> beatPulse{0.0f};
};

struct Star {
  float x;
  float y;
  float speed;
  char glyph;
};

std::array<uint8_t, 7> Glyph(char ch) {
  switch (std::toupper(static_cast<unsigned char>(ch))) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0F};
    case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0F, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0F};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x1F, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x14, 0x04, 0x04, 0x04, 0x1F};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '#': return {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A};
    case '@': return {0x0E, 0x11, 0x15, 0x15, 0x16, 0x10, 0x0E};
    case '*': return {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00};
    case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default: return {0x1F, 0x11, 0x05, 0x02, 0x05, 0x11, 0x1F};
  }
}

void DrawChar(SDL_Renderer* renderer, char ch, int x, int y, int scale, SDL_Color color) {
  const auto rows = Glyph(ch);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int ry = 0; ry < 7; ++ry) {
    for (int rx = 0; rx < 5; ++rx) {
      if (rows[ry] & (1 << (4 - rx))) {
        SDL_Rect px{x + rx * scale, y + ry * scale, scale, scale};
        SDL_RenderFillRect(renderer, &px);
      }
    }
  }
}

void DrawText(SDL_Renderer* renderer, const std::string& text, int x, int y, int scale, SDL_Color color) {
  int cx = x;
  for (char ch : text) {
    DrawChar(renderer, ch, cx, y, scale, color);
    cx += 6 * scale;
  }
}

SDL_Color Hsv(float h, float s, float v, uint8_t a = 255) {
  h = std::fmod(std::fmod(h, 1.0f) + 1.0f, 1.0f);
  const float c = v * s;
  const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
  const float m = v - c;
  float r = 0, g = 0, b = 0;
  if (h < 1.0f / 6.0f) {
    r = c;
    g = x;
  } else if (h < 2.0f / 6.0f) {
    r = x;
    g = c;
  } else if (h < 3.0f / 6.0f) {
    g = c;
    b = x;
  } else if (h < 4.0f / 6.0f) {
    g = x;
    b = c;
  } else if (h < 5.0f / 6.0f) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }
  return SDL_Color{static_cast<uint8_t>((r + m) * 255.0f), static_cast<uint8_t>((g + m) * 255.0f),
                   static_cast<uint8_t>((b + m) * 255.0f), a};
}

void AudioCallback(void* userdata, Uint8* stream, int len) {
  auto* st = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float bpm = 132.0f;
  const float stepDur = 60.0f / bpm / 2.0f;
  const std::array<float, 8> bassNotes = {55.0f, 55.0f, 65.41f, 55.0f, 73.42f, 65.41f, 55.0f, 49.0f};
  const std::array<float, 16> arpNotes = {220.0f, 246.94f, 293.66f, 246.94f, 220.0f, 329.63f, 293.66f, 246.94f,
                                          196.0f, 220.0f, 246.94f, 293.66f, 220.0f, 246.94f, 196.0f, 164.81f};

  for (int i = 0; i < samples; ++i) {
    if (st->t >= st->nextStep) {
      st->kickEnv = 1.0f;
      st->nextStep += stepDur;
      st->step++;
      if ((st->step % 2) == 0) st->beatPulse.store(1.0f, std::memory_order_relaxed);
    }

    const float bassFreq = bassNotes[(st->step / 2) % static_cast<int>(bassNotes.size())];
    const float arpFreq = arpNotes[st->step % static_cast<int>(arpNotes.size())];

    const float kickFreq = 45.0f + st->kickEnv * 180.0f;
    float kick = std::sin(2.0f * kPi * kickFreq * st->t) * st->kickEnv * 0.82f;
    st->kickEnv *= 0.996f;

    st->bassPhase += 2.0f * kPi * bassFreq * dt;
    if (st->bassPhase > 2.0f * kPi) st->bassPhase -= 2.0f * kPi;
    float bassSaw = (st->bassPhase / kPi) - 1.0f;
    bassSaw = std::tanh(bassSaw * 1.7f);
    const float bass = bassSaw * 0.30f;

    st->leadPhase += 2.0f * kPi * arpFreq * dt;
    if (st->leadPhase > 2.0f * kPi) st->leadPhase -= 2.0f * kPi;
    const float lead = ((std::sin(st->leadPhase) > 0.0f) ? 1.0f : -1.0f) * 0.17f;

    float sample = kick + bass + lead;
    sample = std::tanh(sample * 1.25f);
    out[i] = sample * 0.7f;
    st->t += dt;
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
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow("rfds_ascii_oldschool_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWidth, kHeight, SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  AudioState audio;
  SDL_AudioSpec want{};
  want.freq = kSampleRate;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = AudioCallback;
  want.userdata = &audio;
  SDL_AudioSpec have{};
  SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (dev == 0) {
    std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_PauseAudioDevice(dev, 0);

  const std::vector<std::string> logo = {
      " #####   #######  #####   ##### ",
      "##   ##  ##      ##   ## ##   ##",
      "##   ##  #####   ##   ## ##      ",
      "######   ##      ##   ##  #####  ",
      "##  ##   ##      ##   ##      ## ",
      "##   ##  ##      ##   ## ##   ## ",
      "##   ##  ##       #####   #####  ",
  };

  std::vector<Star> stars;
  stars.reserve(140);
  for (int i = 0; i < 140; ++i) {
    Star s{};
    s.x = static_cast<float>(std::rand() % kWidth);
    s.y = static_cast<float>(std::rand() % kHeight);
    s.speed = 18.0f + static_cast<float>(std::rand() % 140);
    const char g[4] = {'.', '+', '*', ':'};
    s.glyph = g[std::rand() % 4];
    stars.push_back(s);
  }

  const std::string scroller =
      "  RFDS ASCII POWER  ::  OLD SCHOOL DEMOSCENE STYLE  ::  COLORS + MOVEMENT + ENERGY  ::  HELLO PARTY PEOPLE!  ";

  const Uint64 start = SDL_GetPerformanceCounter();
  Uint64 prev = start;
  bool running = true;
  bool fullscreen = false;
  float shotAt = -1.0f;
  std::string shotPath;
  ParseShotEnv(shotAt, shotPath);
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

    const Uint64 now = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float t = static_cast<float>((now - start) / freq);
    float dt = static_cast<float>((now - prev) / freq);
    prev = now;
    dt = std::min(dt, 0.04f);

    const float beat = audio.beatPulse.load(std::memory_order_relaxed);
    audio.beatPulse.store(beat * 0.93f, std::memory_order_relaxed);

    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);

    SDL_SetRenderDrawColor(renderer, 2, 2, 9, 255);
    SDL_RenderClear(renderer);

    // Starfield background.
    for (auto& s : stars) {
      s.y += s.speed * dt;
      if (s.y > static_cast<float>(h + 20)) {
        s.y = -10.0f;
        s.x = static_cast<float>(std::rand() % std::max(1, w));
      }
      const float hue = std::fmod(0.56f + s.y / static_cast<float>(h) * 0.45f + t * 0.06f, 1.0f);
      SDL_Color c = Hsv(hue, 0.55f, 0.85f, 170);
      DrawChar(renderer, s.glyph, static_cast<int>(s.x), static_cast<int>(s.y), 2, c);
    }

    // Central RFDS ascii logo.
    const int scale = std::max(2, w / 320);
    const int logoW = static_cast<int>(logo.front().size()) * 6 * scale;
    const int logoH = static_cast<int>(logo.size()) * 8 * scale;
    const int baseX = (w - logoW) / 2;
    const int baseY = (h - logoH) / 2 + static_cast<int>(std::sin(t * 1.8f) * 18.0f);

    for (int row = 0; row < static_cast<int>(logo.size()); ++row) {
      for (int col = 0; col < static_cast<int>(logo[row].size()); ++col) {
        const char ch = logo[row][col];
        if (ch == ' ') continue;
        const float wave = std::sin(t * 4.0f + row * 0.55f + col * 0.15f);
        const int x = baseX + col * 6 * scale + static_cast<int>(wave * 5.0f);
        const int y = baseY + row * 8 * scale + static_cast<int>(std::cos(t * 3.0f + col * 0.2f) * 4.0f);
        const float hue = std::fmod(t * 0.08f + row * 0.05f + col * 0.012f, 1.0f);
        const float val = 0.72f + beat * 0.5f + 0.22f * (wave * 0.5f + 0.5f);
        SDL_Color c = Hsv(hue, 0.85f, std::min(1.0f, val), 255);
        DrawChar(renderer, ch, x, y, scale, c);

        // Glow ghost.
        SDL_Color ghost = c;
        ghost.a = 70;
        DrawChar(renderer, ch, x + 1, y + 1, scale, ghost);
      }
    }

    // Bottom scroller.
    const int scrollScale = std::max(2, scale - 1);
    const int charAdvance = 6 * scrollScale;
    const int textPx = static_cast<int>(scroller.size()) * charAdvance;
    const int scrollX = static_cast<int>(w - std::fmod(t * 180.0f, static_cast<float>(textPx + w)));
    const int baseScrollY = h - 90;

    for (int i = 0; i < static_cast<int>(scroller.size()); ++i) {
      const int x = scrollX + i * charAdvance;
      if (x < -charAdvance || x > w + charAdvance) continue;
      const int y = baseScrollY + static_cast<int>(std::sin(t * 5.0f + i * 0.28f) * 10.0f);
      SDL_Color c = Hsv(std::fmod(0.65f + i * 0.01f + t * 0.10f, 1.0f), 0.78f, 0.95f, 255);
      DrawChar(renderer, scroller[static_cast<size_t>(i)], x, y, scrollScale, c);
    }

    // Top title bar.
    SDL_SetRenderDrawColor(renderer, 10, 10, 24, 220);
    SDL_Rect bar{0, 0, w, 36};
    SDL_RenderFillRect(renderer, &bar);
    DrawText(renderer, "RFDS ASCII OLD SCHOOL INTRO", 20, 8, 3, SDL_Color{180, 230, 255, 255});

    if (shotAt >= 0.0f && t >= shotAt) {
      SaveFrameRenderer(renderer, w, h, shotPath);
      running = false;
    }

    SDL_RenderPresent(renderer);
  }

  SDL_CloseAudioDevice(dev);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
