#include <SDL.h>
#include <OpenGL/gl.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <string>

namespace {
constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kSampleRate = 48000;
constexpr float kPi = 3.14159265359f;

struct AudioState {
  float t = 0.0f;
  float kickEnv = 0.0f;
  float bassPhase = 0.0f;
  float leadPhase = 0.0f;
  float hatSeed = 0.12345f;
  float nextBeat = 0.0f;
  int beatCount = 0;
  std::atomic<float> beatPulse{0.0f};
};

float fract(float x) { return x - std::floor(x); }

float hashNoise(float& seed) {
  seed = fract(seed * 12.9898f + 78.233f);
  return fract(std::sin(seed * 43758.5453f) * 43758.5453f) * 2.0f - 1.0f;
}

void SDLAudioCallback(void* userdata, Uint8* stream, int len) {
  auto* state = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / sizeof(float);
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float bpm = 138.0f;
  const float beatDur = 60.0f / bpm;
  const std::array<float, 8> leadNotes = {110.0f, 123.47f, 146.83f, 164.81f,
                                          174.61f, 146.83f, 123.47f, 98.0f};

  for (int i = 0; i < samples; ++i) {
    if (state->t >= state->nextBeat) {
      state->kickEnv = 1.0f;
      state->nextBeat += beatDur;
      state->beatCount++;
      state->beatPulse.store(1.0f, std::memory_order_relaxed);
    }

    const int step = (state->beatCount / 2) % static_cast<int>(leadNotes.size());
    const float leadFreq = leadNotes[step];
    const float bassFreq = leadNotes[step] * 0.5f;

    const float kickFreq = 44.0f + state->kickEnv * 160.0f;
    float kick = std::sin(2.0f * kPi * kickFreq * state->t) * state->kickEnv;
    kick *= 0.75f;
    state->kickEnv *= 0.9965f;

    state->bassPhase += 2.0f * kPi * bassFreq * dt;
    if (state->bassPhase > 2.0f * kPi) state->bassPhase -= 2.0f * kPi;
    const float bass = std::sin(state->bassPhase) * 0.28f;

    state->leadPhase += 2.0f * kPi * leadFreq * dt;
    if (state->leadPhase > 2.0f * kPi) state->leadPhase -= 2.0f * kPi;
    const float leadSine = std::sin(state->leadPhase);
    const float lead = (leadSine * leadSine * leadSine) * 0.22f;

    const float hatGate = (state->beatCount % 2 == 0) ? 0.0f : 1.0f;
    const float hat = hashNoise(state->hatSeed) * 0.08f * hatGate;

    float sample = kick + bass + lead + hat;
    sample = std::tanh(sample * 1.2f);
    out[i] = sample * 0.7f;
    state->t += dt;
  }
}

void CheckShader(GLuint shader, const char* stage) {
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok == GL_TRUE) return;

  GLint logLen = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
  std::string log(logLen > 1 ? static_cast<size_t>(logLen) : 1, '\0');
  glGetShaderInfoLog(shader, logLen, nullptr, log.data());
  std::fprintf(stderr, "%s shader compile failed:\n%s\n", stage, log.c_str());
  std::exit(1);
}

void CheckProgram(GLuint program) {
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (ok == GL_TRUE) return;

  GLint logLen = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
  std::string log(logLen > 1 ? static_cast<size_t>(logLen) : 1, '\0');
  glGetProgramInfoLog(program, logLen, nullptr, log.data());
  std::fprintf(stderr, "Program link failed:\n%s\n", log.c_str());
  std::exit(1);
}

GLuint BuildProgram() {
  const char* vsrc = R"(
    #version 120
    void main() {
      gl_TexCoord[0] = gl_MultiTexCoord0;
      gl_Position = ftransform();
    }
  )";

  const char* fsrc = R"(
    #version 120
    uniform vec2 iResolution;
    uniform float iTime;
    uniform float iBeat;
    uniform int iScene;

    float hash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }

    float noise(vec2 p) {
      vec2 i = floor(p);
      vec2 f = fract(p);
      vec2 u = f * f * (3.0 - 2.0 * f);
      return mix(
        mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
        u.y
      );
    }

    float sdBox(vec2 p, vec2 b) {
      vec2 d = abs(p) - b;
      return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    }

    vec3 renderSceneOne(vec2 frag) {
      vec2 uv = (frag * 2.0 - iResolution.xy) / iResolution.y;
      float t = iTime;
      float r = length(uv);
      float a = atan(uv.y, uv.x);
      float pulse = smoothstep(0.0, 0.6, iBeat);

      float rings = sin(22.0 * r - t * 5.0 + sin(a * 6.0 + t * 2.0));
      float tunnel = sin(14.0 * (a + r) + t * 2.2);
      float n = noise(uv * 5.0 + vec2(t * 0.25, -t * 0.18));

      float glow = exp(-4.0 * r) * (0.8 + pulse * 0.7);
      vec3 base = vec3(0.06, 0.02, 0.10);
      vec3 colA = vec3(0.16, 0.85, 0.95);
      vec3 colB = vec3(0.95, 0.18, 0.65);

      vec3 col = base;
      col += colA * (0.5 + 0.5 * rings) * (0.55 + 0.45 * tunnel);
      col += colB * (0.5 + 0.5 * sin(8.0 * r - t * 3.0 + a * 3.0));
      col += vec3(0.4, 0.7, 1.0) * glow;
      col += vec3(0.10, 0.08, 0.12) * n;
      col *= 1.0 - smoothstep(0.9, 1.4, r);
      return pow(max(col, 0.0), vec3(0.9));
    }

    vec3 renderSceneTwo(vec2 frag) {
      vec3 col = vec3(0.01);

      // Left code-like panel with scrolling pseudo-glyphs.
      float codeMask = 1.0 - step(iResolution.x * 0.54, frag.x);
      vec2 codePos = vec2(frag.x * 0.24, (frag.y + iTime * 75.0) * 0.095);
      vec2 cell = floor(codePos);
      vec2 fracCell = fract(codePos);
      float rowNoise = hash(vec2(cell.y, floor(cell.y * 0.25)));
      float charOn = step(0.33, hash(cell + vec2(19.0, 7.0)));
      float lineOn = step(0.18, rowNoise);
      float glyph = charOn * lineOn;
      float pixel = step(0.22, fracCell.x) * step(0.30, fracCell.y) * step(fracCell.y, 0.88);
      float ink = glyph * pixel;
      float gutterFade = 1.0 - smoothstep(iResolution.x * 0.44, iResolution.x * 0.56, frag.x);
      vec3 codeCol = vec3(0.14, 0.25, 0.92) * (0.20 + 0.80 * ink) * gutterFade;
      col += codeCol * codeMask;

      // Center-right emblem.
      vec2 c = (frag - vec2(iResolution.x * 0.62, iResolution.y * 0.56)) / iResolution.y;
      float radius = 0.22;
      float inCircle = 1.0 - step(radius, length(c));
      vec3 emblem = vec3(1.0);
      if (c.y > -0.02) emblem = vec3(0.58);

      // Crown cut: three triangular notches in the upper gray half.
      vec2 crown = c;
      crown.y += 0.02;
      float notch = 0.0;
      for (int i = 0; i < 3; ++i) {
        float cx = (float(i) - 1.0) * 0.085;
        vec2 tp = crown - vec2(cx, 0.03);
        float tri = step(abs(tp.x) * 1.25 + 0.03, tp.y + 0.12) * step(tp.y, 0.09);
        notch = max(notch, tri);
      }
      emblem = mix(emblem, vec3(1.0), notch * step(-0.02, c.y));
      col = mix(col, emblem, inCircle);

      // Top HUD wedge.
      vec2 h = (frag - vec2(iResolution.x * 0.62, iResolution.y * 0.08)) / iResolution.y;
      float hudW = 0.20 - h.y * 0.45;
      float hud = step(abs(h.x), hudW) * step(abs(h.y), 0.065);
      col = mix(col, vec3(1.0), hud);

      // Dark "13:30" strip approximation.
      float display = step(sdBox(h - vec2(0.0, -0.004), vec2(0.10, 0.018)), 0.0);
      col = mix(col, vec3(0.05), display * hud);
      float bars = 0.0;
      for (int i = 0; i < 9; ++i) {
        float bx = -0.082 + float(i) * 0.02;
        float bar = step(sdBox(h - vec2(bx, -0.004), vec2(0.0035, 0.013)), 0.0);
        bars = max(bars, bar * step(0.1, hash(vec2(float(i), 3.0))));
      }
      col = mix(col, vec3(0.95), bars * hud);

      // Lower title block approximation.
      float title = step(sdBox(h - vec2(0.0, 0.055), vec2(0.073, 0.016)), 0.0);
      col = mix(col, vec3(0.03), title);
      float titleGaps = 0.0;
      for (int i = 0; i < 8; ++i) {
        float tx = -0.058 + float(i) * 0.016;
        float g = step(sdBox(h - vec2(tx, 0.055), vec2(0.0045, 0.010)), 0.0);
        titleGaps = max(titleGaps, g * step(0.2, hash(vec2(float(i), 9.0))));
      }
      col = mix(col, vec3(0.96), titleGaps * title);

      // Vignette and beat flash.
      vec2 uv = (frag * 2.0 - iResolution.xy) / iResolution.y;
      float vignette = 1.0 - smoothstep(0.8, 1.35, length(uv));
      col *= 0.25 + 0.75 * vignette;
      col += vec3(iBeat * 0.08);
      return col;
    }

    void main() {
      vec3 col = (iScene == 2) ? renderSceneTwo(gl_FragCoord.xy) : renderSceneOne(gl_FragCoord.xy);
      gl_FragColor = vec4(col, 1.0);
    }
  )";

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vsrc, nullptr);
  glCompileShader(vs);
  CheckShader(vs, "Vertex");

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &fsrc, nullptr);
  glCompileShader(fs);
  CheckShader(fs, "Fragment");

  GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);
  CheckProgram(program);

  glDeleteShader(vs);
  glDeleteShader(fs);
  return program;
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

void SaveFrameGL(int w, int h, const std::string& path) {
  std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (!s) return;
  for (int y = 0; y < h; ++y) {  // GL rows are bottom-up
    std::memcpy(static_cast<unsigned char*>(s->pixels) + static_cast<size_t>(y) * s->pitch,
                px.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4);
  }
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

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_Window* window = SDL_CreateWindow("mac_demo_intro", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, kWidth, kHeight,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (!context) {
    std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);

  AudioState audioState;
  SDL_AudioSpec desired{};
  desired.freq = kSampleRate;
  desired.format = AUDIO_F32SYS;
  desired.channels = 1;
  desired.samples = 1024;
  desired.callback = SDLAudioCallback;
  desired.userdata = &audioState;

  SDL_AudioSpec obtained{};
  SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if (audioDev == 0) {
    std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_PauseAudioDevice(audioDev, 0);

  GLuint program = BuildProgram();
  const GLint iResLoc = glGetUniformLocation(program, "iResolution");
  const GLint iTimeLoc = glGetUniformLocation(program, "iTime");
  const GLint iBeatLoc = glGetUniformLocation(program, "iBeat");
  const GLint iSceneLoc = glGetUniformLocation(program, "iScene");

  Uint64 start = SDL_GetPerformanceCounter();
  bool running = true;
  bool fullscreen = false;
  int scene = 1;
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
        if (e.key.keysym.sym == SDLK_1) scene = 1;
        if (e.key.keysym.sym == SDLK_2) scene = 2;
      }
    }

    int w = 0;
    int h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    const Uint64 now = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float timeSec = static_cast<float>((now - start) / freq);
    const float beatPulse = audioState.beatPulse.load(std::memory_order_relaxed);
    audioState.beatPulse.store(beatPulse * 0.94f, std::memory_order_relaxed);

    glUseProgram(program);
    glUniform2f(iResLoc, static_cast<float>(w), static_cast<float>(h));
    glUniform1f(iTimeLoc, timeSec);
    glUniform1f(iBeatLoc, beatPulse);
    glUniform1i(iSceneLoc, scene);

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(-1.0f, -1.0f);
    glVertex2f(1.0f, -1.0f);
    glVertex2f(-1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glEnd();

    if (shotAt >= 0.0f && timeSec >= shotAt) {
      SaveFrameGL(w, h, shotPath);
      running = false;
    }

    SDL_GL_SwapWindow(window);
  }

  SDL_CloseAudioDevice(audioDev);
  glDeleteProgram(program);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
