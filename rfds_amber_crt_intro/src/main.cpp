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
constexpr float kBpm = 125.0f;
constexpr float kPi = 3.14159265359f;

struct AudioState {
  float t = 0.0f;
  float kickEnv = 0.0f;
  float snareEnv = 0.0f;
  float hatEnv = 0.0f;
  float hatLastNoise = 0.0f;
  float stabEnv = 0.0f;
  float stabPhase = 0.0f;
  float stabFreq = 440.0f;
  float bassPhase = 0.0f;
  float bassFreq = 55.0f;
  float bassTarget = 55.0f;
  float noiseSeed = 0.123f;
  float nextStep = 0.0f;
  int step16 = 0;
  std::atomic<float> beatPulse{0.0f};
  std::atomic<int> step{0};
};

float Fract(float x) { return x - std::floor(x); }

float NoiseStep(float& seed) {
  seed = Fract(seed * 12.9898f + 78.233f);
  return Fract(std::sin(seed * 43758.5453f) * 43758.5453f) * 2.0f - 1.0f;
}

void SDLAudioCallback(void* userdata, Uint8* stream, int len) {
  auto* state = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float beatDur = 60.0f / kBpm;
  const float stepDur = beatDur * 0.25f;

  const std::array<int, 16> kickPattern = {
      1, 0, 0, 0, 1, 0, 1, 0,
      1, 0, 0, 1, 1, 0, 0, 0};
  const std::array<float, 16> bassLine = {
      55.0f, 0.0f, 55.0f, 65.41f, 73.42f, 0.0f, 65.41f, 55.0f,
      49.0f, 0.0f, 55.0f, 65.41f, 73.42f, 65.41f, 55.0f, 0.0f};
  const std::array<float, 4> stepStabs = {523.25f, 659.25f, 783.99f, 659.25f};

  for (int i = 0; i < samples; ++i) {
    if (state->t >= state->nextStep) {
      const int s = state->step16 % 16;

      if (kickPattern[s] == 1) state->kickEnv = 1.0f;
      if (s == 4 || s == 12) state->snareEnv = 1.0f;

      // 16th hats with slight dynamic differences.
      state->hatEnv = (s % 2 == 0) ? 0.35f : 0.75f;

      // Sync RFDS letter step every quarter note.
      if ((s % 4) == 0) {
        const int letterStep = (s / 4) % 4;
        state->step.store(letterStep, std::memory_order_relaxed);
        state->stabEnv = 1.0f;
        state->stabFreq = stepStabs[letterStep];
        state->beatPulse.store(1.0f, std::memory_order_relaxed);
      }

      const float note = bassLine[s];
      if (note > 1.0f) state->bassTarget = note;

      state->step16++;
      // Slight swing on odd 16ths for funk groove.
      const bool oddStep = (state->step16 % 2) == 1;
      state->nextStep += stepDur * (oddStep ? 1.10f : 0.90f);
    }

    const float kickFreq = 42.0f + state->kickEnv * 170.0f;
    float kick = std::sin(2.0f * kPi * kickFreq * state->t) * state->kickEnv * 0.88f;
    state->kickEnv *= 0.9962f;

    // Snare/clap style: noise burst + short tone.
    const float snareNoise = NoiseStep(state->noiseSeed);
    const float snareTone = std::sin(2.0f * kPi * 190.0f * state->t) * 0.35f;
    const float snare = (snareNoise * 0.75f + snareTone) * state->snareEnv * 0.42f;
    state->snareEnv *= 0.982f;

    // Slight glide for bass line.
    state->bassFreq += (state->bassTarget - state->bassFreq) * 0.0024f;
    state->bassPhase += 2.0f * kPi * state->bassFreq * dt;
    if (state->bassPhase > 2.0f * kPi) state->bassPhase -= 2.0f * kPi;
    const float bassSub = std::sin(state->bassPhase);
    const float bassSq = (bassSub > 0.0f ? 1.0f : -1.0f) * 0.45f;
    const float bass = std::tanh((bassSub * 0.9f + bassSq) * 1.2f) * 0.34f;

    // Short chord stab on RFDS step hits.
    state->stabPhase += 2.0f * kPi * state->stabFreq * dt;
    if (state->stabPhase > 2.0f * kPi) state->stabPhase -= 2.0f * kPi;
    const float stab = std::sin(state->stabPhase) * state->stabEnv * 0.19f;
    state->stabEnv *= 0.989f;

    // Crisper hats by differentiating noise.
    const float rawHat = NoiseStep(state->noiseSeed);
    const float hatHP = rawHat - state->hatLastNoise * 0.92f;
    state->hatLastNoise = rawHat;
    const float hat = hatHP * state->hatEnv * 0.18f;
    state->hatEnv *= 0.97f;

    const float duck = 1.0f - state->kickEnv * 0.30f;
    float sample = kick + snare + (bass + stab + hat) * duck;
    sample = std::tanh(sample * 1.25f);
    out[i] = sample * 0.72f;
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
    uniform int iStep;
    uniform float iBeatPhase;

    float hash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }

    float sdBox(vec2 p, vec2 b) {
      vec2 d = abs(p) - b;
      return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    }

    float stroke(vec2 p, vec2 c, vec2 b) {
      return 1.0 - smoothstep(0.0, 0.005, sdBox(p - c, b));
    }

    float glyphR(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.35, 0.0), vec2(0.09, 0.48));
      g += stroke(p, vec2(0.00, 0.38), vec2(0.23, 0.09));
      g += stroke(p, vec2(0.19, 0.12), vec2(0.09, 0.17));
      g += stroke(p, vec2(0.13, -0.18), vec2(0.08, 0.08));
      g += stroke(p, vec2(0.27, -0.32), vec2(0.08, 0.08));
      return clamp(g, 0.0, 1.0);
    }

    float glyphF(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.34, 0.0), vec2(0.09, 0.48));
      g += stroke(p, vec2(0.02, 0.38), vec2(0.30, 0.09));
      g += stroke(p, vec2(-0.02, 0.05), vec2(0.24, 0.08));
      return clamp(g, 0.0, 1.0);
    }

    float glyphD(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.34, 0.0), vec2(0.09, 0.48));
      g += stroke(p, vec2(0.02, 0.38), vec2(0.22, 0.09));
      g += stroke(p, vec2(0.02, -0.38), vec2(0.22, 0.09));
      g += stroke(p, vec2(0.25, 0.0), vec2(0.09, 0.29));
      return clamp(g, 0.0, 1.0);
    }

    float glyphS(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(0.0, 0.38), vec2(0.32, 0.09));
      g += stroke(p, vec2(-0.24, 0.15), vec2(0.09, 0.15));
      g += stroke(p, vec2(0.0, 0.0), vec2(0.30, 0.08));
      g += stroke(p, vec2(0.24, -0.15), vec2(0.09, 0.15));
      g += stroke(p, vec2(0.0, -0.38), vec2(0.32, 0.09));
      return clamp(g, 0.0, 1.0);
    }

    float glyphW(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.33, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(0.33, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(-0.10, -0.22), vec2(0.08, 0.18));
      g += stroke(p, vec2(0.10, -0.22), vec2(0.08, 0.18));
      return clamp(g, 0.0, 1.0);
    }

    float glyphA(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.23, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(0.23, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(0.0, 0.38), vec2(0.28, 0.09));
      g += stroke(p, vec2(0.0, 0.04), vec2(0.22, 0.08));
      return clamp(g, 0.0, 1.0);
    }

    float glyphH(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.28, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(0.28, 0.0), vec2(0.08, 0.48));
      g += stroke(p, vec2(0.0, 0.0), vec2(0.24, 0.08));
      return clamp(g, 0.0, 1.0);
    }

    float glyphE(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.31, 0.0), vec2(0.09, 0.48));
      g += stroke(p, vec2(0.02, 0.38), vec2(0.28, 0.09));
      g += stroke(p, vec2(-0.02, 0.0), vec2(0.22, 0.08));
      g += stroke(p, vec2(0.02, -0.38), vec2(0.28, 0.09));
      return clamp(g, 0.0, 1.0);
    }

    float glyphFromId(vec2 p, int id) {
      if (id == 0) return glyphR(p);
      if (id == 1) return glyphF(p);
      if (id == 2) return glyphD(p);
      if (id == 3) return glyphS(p);
      if (id == 4) return glyphW(p);
      if (id == 5) return glyphA(p);
      if (id == 6) return glyphH(p);
      if (id == 7) return glyphE(p);
      return 0.0;
    }

    float drawGlyph(vec2 uv, vec2 center, float scale, int id, float lit, float wobble) {
      vec2 p = (uv - center) / scale;
      p.y += sin(p.x * 10.0 + iTime * 8.0) * wobble;
      float m = glyphFromId(p, id);
      float glow = m * 0.35;
      return m * lit + glow;
    }

    void main() {
      vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
      float t = iTime;
      vec3 col = vec3(0.02, 0.015, 0.01);

      float scan = 0.9 + 0.1 * sin(gl_FragCoord.y * 1.8 + t * 3.0);
      float barrel = 1.0 - smoothstep(0.7, 1.25, length(uv));
      col += vec3(0.10, 0.07, 0.03) * scan * barrel;

      // Static noise.
      vec2 ncell = floor(gl_FragCoord.xy * 0.6 + vec2(t * 40.0, 0.0));
      float n = hash(ncell);
      col += vec3(0.05, 0.03, 0.01) * step(0.985, n);

      // RFDS main letters, active step pops.
      float baseX = -0.42;
      float spacing = 0.28;
      float scale = 0.2;
      for (int i = 0; i < 4; ++i) {
        float active = (i == iStep) ? 1.0 : 0.0;
        float wobble = 0.01 + 0.035 * active * (1.0 - iBeatPhase);
        float lit = 0.55 + 0.45 * active + iBeat * 0.25;
        vec2 c = vec2(baseX + float(i) * spacing, 0.08);
        c.y += sin(t * 6.0 + float(i) * 1.5) * 0.01;
        float g = drawGlyph(uv, c, scale * (1.0 + 0.1 * active * (1.0 - iBeatPhase)), i, lit, wobble);
        vec3 amber = vec3(1.0, 0.72, 0.24);
        vec3 hot = vec3(1.0, 0.88, 0.50);
        col += mix(amber, hot, active) * g;
      }

      // Sine scroller "RFDS WAS HERE" moving right-to-left.
      float scroll = mod(t * 0.45, 3.1);
      float x = 1.45 - scroll;
      float y0 = -0.56;
      float tiny = 0.08;
      int seq[13];
      seq[0] = 0;   // R
      seq[1] = 1;   // F
      seq[2] = 2;   // D
      seq[3] = 3;   // S
      seq[4] = -1;  // space
      seq[5] = 4;   // W
      seq[6] = 5;   // A
      seq[7] = 3;   // S
      seq[8] = -1;  // space
      seq[9] = 6;   // H
      seq[10] = 7;  // E
      seq[11] = 0;  // R
      seq[12] = 7;  // E
      for (int i = 0; i < 13; ++i) {
        if (seq[i] >= 0) {
          vec2 c = vec2(x, y0 + sin((x + t) * 6.0) * 0.045);
          float g = drawGlyph(uv, c, tiny, seq[i], 0.95, 0.0);
          col += vec3(0.85, 0.58, 0.20) * g;
        }
        x += 0.12;
      }

      // Top raster-ish bars.
      float topBand = step(0.74, uv.y);
      float bars = 0.5 + 0.5 * sin((uv.x + t * 0.9) * 20.0);
      col += vec3(0.45, 0.25, 0.09) * bars * topBand * 0.45;

      // CRT vignette and beat flash.
      float vignette = 1.0 - smoothstep(0.8, 1.35, length(uv));
      col *= 0.35 + 0.65 * vignette;
      col += vec3(0.15, 0.10, 0.04) * iBeat * 0.45;
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

  SDL_Window* window = SDL_CreateWindow("rfds_amber_crt_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWidth, kHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
  SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if (audio == 0) {
    std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_PauseAudioDevice(audio, 0);

  GLuint program = BuildProgram();
  const GLint iResLoc = glGetUniformLocation(program, "iResolution");
  const GLint iTimeLoc = glGetUniformLocation(program, "iTime");
  const GLint iBeatLoc = glGetUniformLocation(program, "iBeat");
  const GLint iStepLoc = glGetUniformLocation(program, "iStep");
  const GLint iBeatPhaseLoc = glGetUniformLocation(program, "iBeatPhase");

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
    SDL_GL_GetDrawableSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    const Uint64 now = SDL_GetPerformanceCounter();
    const double perf = static_cast<double>(SDL_GetPerformanceFrequency());
    const float timeSec = static_cast<float>((now - start) / perf);
    const float beatPulse = audioState.beatPulse.load(std::memory_order_relaxed);
    const int step = audioState.step.load(std::memory_order_relaxed);
    const float beatPos = timeSec * (kBpm / 60.0f);
    const float beatPhase = beatPos - std::floor(beatPos);
    audioState.beatPulse.store(beatPulse * 0.93f, std::memory_order_relaxed);

    glUseProgram(program);
    glUniform2f(iResLoc, static_cast<float>(w), static_cast<float>(h));
    glUniform1f(iTimeLoc, timeSec);
    glUniform1f(iBeatLoc, beatPulse);
    glUniform1i(iStepLoc, step);
    glUniform1f(iBeatPhaseLoc, beatPhase);

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

  SDL_CloseAudioDevice(audio);
  glDeleteProgram(program);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
