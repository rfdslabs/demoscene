#include <SDL.h>

#include <algorithm>
#include <array>
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
constexpr float kBpm = 125.0f;
constexpr float kPi = 3.14159265359f;

struct AudioState {
  float t = 0.0f;
  float padPhaseA = 0.0f;
  float padPhaseB = 0.0f;
  float padPhaseC = 0.0f;
  float bassPhase = 0.0f;
  float chimeEnv = 0.0f;
  float chimePhase = 0.0f;
  float chimeFreq = 440.0f;
  float swell = 0.0f;
  float noiseSeed = 0.17f;
  float nextBeat = 0.0f;
  int beat = 0;
};

struct ScriptLine {
  float start;
  float cps;
  std::string text;
};

float Fract(float x) { return x - std::floor(x); }

float Noise(float& seed) {
  seed = Fract(seed * 12.9898f + 78.233f);
  return Fract(std::sin(seed * 43758.5453f) * 43758.5453f) * 2.0f - 1.0f;
}

void AudioCallback(void* userdata, Uint8* stream, int len) {
  auto* st = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float beatDur = 60.0f / 84.0f;  // calm winter pacing
  const std::array<float, 8> rootNotes = {220.0f, 246.94f, 261.63f, 293.66f, 261.63f, 246.94f, 220.0f, 196.0f};
  const std::array<float, 4> chimeIntervals = {2.0f, 2.5f, 3.0f, 4.0f};

  for (int i = 0; i < samples; ++i) {
    if (st->t >= st->nextBeat) {
      const int idx = st->beat % static_cast<int>(rootNotes.size());
      const float root = rootNotes[idx];
      st->chimeFreq = root * chimeIntervals[st->beat % static_cast<int>(chimeIntervals.size())];
      st->chimeEnv = 1.0f;
      st->swell = 1.0f;
      st->nextBeat += beatDur;
      st->beat++;
    }

    const int idx = st->beat % static_cast<int>(rootNotes.size());
    const float root = rootNotes[idx];
    const float fifth = root * 1.5f;
    const float octave = root * 2.0f;

    st->padPhaseA += 2.0f * kPi * root * dt;
    st->padPhaseB += 2.0f * kPi * fifth * dt;
    st->padPhaseC += 2.0f * kPi * octave * dt;
    if (st->padPhaseA > 2.0f * kPi) st->padPhaseA -= 2.0f * kPi;
    if (st->padPhaseB > 2.0f * kPi) st->padPhaseB -= 2.0f * kPi;
    if (st->padPhaseC > 2.0f * kPi) st->padPhaseC -= 2.0f * kPi;

    const float pad = (std::sin(st->padPhaseA) * 0.55f + std::sin(st->padPhaseB) * 0.30f +
                       std::sin(st->padPhaseC) * 0.15f) *
                      0.28f;

    st->bassPhase += 2.0f * kPi * (root * 0.5f) * dt;
    if (st->bassPhase > 2.0f * kPi) st->bassPhase -= 2.0f * kPi;
    const float bass = std::sin(st->bassPhase) * 0.17f;

    st->chimePhase += 2.0f * kPi * st->chimeFreq * dt;
    if (st->chimePhase > 2.0f * kPi) st->chimePhase -= 2.0f * kPi;
    const float chime = std::sin(st->chimePhase) * st->chimeEnv * 0.13f;
    st->chimeEnv *= 0.9955f;

    const float airy = Noise(st->noiseSeed) * 0.012f;
    st->swell *= 0.996f;

    float sample = pad + bass + chime + airy + std::sin(st->t * 0.35f) * st->swell * 0.02f;
    sample = std::tanh(sample * 1.1f);
    out[i] = sample * 0.62f;
    st->t += dt;
  }
}

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
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    case ',': return {0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08};
    case '%': return {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
    case ':': return {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    case '#': return {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A};
    case '$': return {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04};
    case '>': return {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10};
    case '_': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default: return {0x1F, 0x11, 0x05, 0x02, 0x05, 0x11, 0x1F};  // unknown
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

int TypedCharsForElapsed(const ScriptLine& line, float elapsed) {
  if (elapsed <= 0.0f) return 0;
  float t = 0.0f;
  int shown = 0;
  for (int i = 0; i < static_cast<int>(line.text.size()); ++i) {
    const char ch = line.text[static_cast<size_t>(i)];
    float d = 1.0f / line.cps;
    if (ch == ' ') d += 0.03f;
    if (ch == '.' || ch == ':' || ch == '/' || ch == '-') d += 0.05f;
    if (ch == '\b') d *= 0.75f;
    if ((i % 19) == 18) d += 0.09f;  // brief "thinking" pauses
    t += d;
    if (t > elapsed) break;
    shown++;
  }
  return shown;
}

std::string BuildVisibleText(const std::string& raw, int typedChars) {
  std::string out;
  const int count = std::clamp(typedChars, 0, static_cast<int>(raw.size()));
  out.reserve(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const char ch = raw[static_cast<size_t>(i)];
    if (ch == '\b') {
      if (!out.empty()) out.pop_back();
    } else {
      out.push_back(ch);
    }
  }
  return out;
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

  SDL_Window* window = SDL_CreateWindow("crt_unix_session_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

  const std::vector<ScriptLine> script = {
      {0.2f, 35.0f, "$ WHOAMI"},
      {1.2f, 30.0f, "RFDS"},
      {2.0f, 35.0f, "$ UNAME -A"},
      {3.0f, 40.0f, "DARWIN DEMOSCENE 25.3.0 ARM64"},
      {4.0f, 32.0f, "$ DIG EXAMLPE\b\b\bPLE.COM +SHORT"},
      {5.2f, 26.0f, "203.0.113.10"},
      {5.9f, 26.0f, "203.0.113.11"},
      {6.7f, 31.0f, "$ NSLOOKUP EXAMPLE.CMO\b\bOM"},
      {7.9f, 32.0f, "SERVER: 1.1.1.1"},
      {8.6f, 32.0f, "ADDRESS: 1.1.1.1#53"},
      {9.3f, 32.0f, "NAME: EXAMPLE.COM"},
      {10.0f, 32.0f, "ADDRESS: 203.0.113.10"},
      {10.8f, 33.0f, "$ PING -C 1 EXAMPLE.COM"},
      {12.2f, 32.0f, "1 PACKETS TRANSMITTED, 1 RECEIVED, 0% LOSS"},
      {13.6f, 34.0f, "$ _"},
  };

  bool running = true;
  bool fullscreen = false;
  const Uint64 start = SDL_GetPerformanceCounter();

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

    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    const float sx = static_cast<float>(w) / static_cast<float>(kWidth);
    const float sy = static_cast<float>(h) / static_cast<float>(kHeight);
    const float u = std::min(sx, sy);
    const int monitorW = static_cast<int>(980 * u);
    const int monitorH = static_cast<int>(610 * u);
    const int monitorX = (w - monitorW) / 2;
    const int monitorY = (h - monitorH) / 2 - static_cast<int>(8 * u);
    const int screenMargin = static_cast<int>(42 * u);
    const int screenX = monitorX + screenMargin;
    const int screenY = monitorY + screenMargin;
    const int screenW = monitorW - screenMargin * 2;
    const int screenH = monitorH - screenMargin * 2;

    const Uint64 now = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float t = static_cast<float>((now - start) / freq);

    SDL_SetRenderDrawColor(renderer, 6, 6, 7, 255);
    SDL_RenderClear(renderer);

    // Monitor body.
    SDL_SetRenderDrawColor(renderer, 52, 56, 62, 255);
    SDL_Rect body{monitorX, monitorY, monitorW, monitorH};
    SDL_RenderFillRect(renderer, &body);

    SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255);
    SDL_Rect bezel{monitorX + static_cast<int>(10 * u), monitorY + static_cast<int>(10 * u),
                   monitorW - static_cast<int>(20 * u), monitorH - static_cast<int>(20 * u)};
    SDL_RenderFillRect(renderer, &bezel);

    // CRT screen.
    SDL_SetRenderDrawColor(renderer, 6, 12, 10, 255);
    SDL_Rect screen{screenX, screenY, screenW, screenH};
    SDL_RenderFillRect(renderer, &screen);

    // Xterm-like dark phosphor background.
    for (int y = 0; y < screenH; ++y) {
      const float fy = static_cast<float>(y) / static_cast<float>(screenH);
      const uint8_t line = static_cast<uint8_t>(10 + 12 * (1.0f - std::abs(fy - 0.5f) * 1.4f));
      SDL_SetRenderDrawColor(renderer, 0, line, static_cast<uint8_t>(line * 0.6f), 255);
      SDL_RenderDrawLine(renderer, screenX, screenY + y, screenX + screenW - 1, screenY + y);
    }

    // Scanlines.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
    for (int y = 0; y < screenH; y += 3) {
      SDL_RenderDrawLine(renderer, screenX, screenY + y, screenX + screenW - 1, screenY + y);
    }

    // Terminal text.
    const int textScale = std::max(1, static_cast<int>(2 * u));
    const int charH = 7 * textScale;
    int ty = screenY + static_cast<int>(20 * u);
    const int tx = screenX + static_cast<int>(20 * u);
    SDL_Color mainColor{126, 252, 122, 255};
    SDL_Color dimColor{50, 120, 58, 255};

    int lastVisibleLine = -1;
    int lastVisibleChars = 0;
    for (int i = 0; i < static_cast<int>(script.size()); ++i) {
      const auto& line = script[i];
      if (t < line.start) break;

      const float elapsed = t - line.start;
      const int visibleChars = TypedCharsForElapsed(line, elapsed);
      const std::string shown = BuildVisibleText(line.text, visibleChars);
      DrawText(renderer, shown, tx, ty, textScale, mainColor);

      // Add ghost line for CRT persistence.
      SDL_Color ghost = dimColor;
      ghost.a = 120;
      DrawText(renderer, shown, tx + 1, ty + 1, textScale, ghost);

      lastVisibleLine = i;
      lastVisibleChars = static_cast<int>(shown.size());
      ty += charH + static_cast<int>(10 * u);
      if (ty > screenY + screenH - charH - static_cast<int>(20 * u)) break;
    }

    // Steady (non-blinking) xterm-style cursor.
    if (lastVisibleLine >= 0) {
      int cursorY = screenY + static_cast<int>(20 * u) + lastVisibleLine * (charH + static_cast<int>(10 * u));
      int cursorX = tx + lastVisibleChars * (6 * textScale);
      SDL_SetRenderDrawColor(renderer, 126, 252, 122, 255);
      SDL_Rect cur{cursorX, cursorY + charH - std::max(1, textScale), 5 * textScale, std::max(1, textScale)};
      SDL_RenderFillRect(renderer, &cur);
    }

    // Top terminal title.
    SDL_Color title{120, 175, 120, 255};
    DrawText(renderer, "UNIX SESSION :: RFDS TERMINAL", screenX + static_cast<int>(14 * u),
             screenY + static_cast<int>(6 * u), std::max(1, textScale - 1), title);

    // Screen glare.
    SDL_SetRenderDrawColor(renderer, 90, 110, 90, 26);
    SDL_Rect glare{screenX + static_cast<int>(40 * u), screenY + static_cast<int>(20 * u),
                   static_cast<int>(screenW * 0.35f), static_cast<int>(screenH * 0.08f)};
    SDL_RenderFillRect(renderer, &glare);

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
