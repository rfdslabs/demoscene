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
constexpr float kBpm = 142.0f;
constexpr float kPi = 3.14159265359f;

struct AudioState {
  float t = 0.0f;
  float kickEnv = 0.0f;
  float snareEnv = 0.0f;
  float bassPhase = 0.0f;
  float leadPhase = 0.0f;
  float blipPhase = 0.0f;
  float blipEnv = 0.0f;
  float blipFreq = 440.0f;
  float noiseSeed = 0.317f;
  float nextBeat = 0.0f;
  int beat = 0;
  std::atomic<float> beatPulse{0.0f};
  std::atomic<int> letterStep{0};
};

float Fract(float x) { return x - std::floor(x); }

float NoiseStep(float& seed) {
  seed = Fract(seed * 13.127f + 0.173f);
  return Fract(std::sin(seed * 52341.321f) * 43213.17f) * 2.0f - 1.0f;
}

void SDLAudioCallback(void* userdata, Uint8* stream, int len) {
  auto* state = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float beatDur = 60.0f / kBpm;
  const std::array<float, 8> bassNotes = {55.0f, 55.0f, 65.41f, 55.0f, 73.42f, 65.41f, 55.0f, 49.0f};
  const std::array<float, 4> letterNotes = {523.25f, 659.25f, 783.99f, 987.77f};  // R F D S

  for (int i = 0; i < samples; ++i) {
    if (state->t >= state->nextBeat) {
      state->kickEnv = 1.0f;
      if ((state->beat % 4) == 1 || (state->beat % 4) == 3) state->snareEnv = 1.0f;
      state->nextBeat += beatDur;
      const int step = state->beat % 4;
      state->letterStep.store(step, std::memory_order_relaxed);
      state->blipEnv = 1.0f;
      state->blipFreq = letterNotes[step];
      state->beat++;
      state->beatPulse.store(1.0f, std::memory_order_relaxed);
    }

    const int bassStep = state->beat % static_cast<int>(bassNotes.size());
    const float bassFreq = bassNotes[bassStep];

    const float kickFreq = 45.0f + state->kickEnv * 180.0f;
    float kick = std::sin(2.0f * kPi * kickFreq * state->t) * state->kickEnv;
    kick *= 0.86f;
    state->kickEnv *= 0.9958f;

    float snare = NoiseStep(state->noiseSeed) * state->snareEnv;
    snare *= 0.28f;
    state->snareEnv *= 0.985f;

    state->bassPhase += 2.0f * kPi * bassFreq * dt;
    if (state->bassPhase >= 2.0f * kPi) state->bassPhase -= 2.0f * kPi;
    float bassSaw = (state->bassPhase / kPi) - 1.0f;
    bassSaw = std::tanh(bassSaw * 1.8f);
    const float bass = bassSaw * 0.34f;

    state->leadPhase += 2.0f * kPi * (bassFreq * 2.0f) * dt;
    if (state->leadPhase >= 2.0f * kPi) state->leadPhase -= 2.0f * kPi;
    const float leadSquare = (std::sin(state->leadPhase) > 0.0f) ? 1.0f : -1.0f;
    const float lead = leadSquare * 0.11f;

    state->blipPhase += 2.0f * kPi * state->blipFreq * dt;
    if (state->blipPhase >= 2.0f * kPi) state->blipPhase -= 2.0f * kPi;
    const float blip = std::sin(state->blipPhase) * state->blipEnv * 0.24f;
    state->blipEnv *= 0.988f;

    const float hatGate = (std::fmod(state->t * (kBpm / 60.0f) * 2.0f, 1.0f) > 0.5f) ? 1.0f : 0.0f;
    const float hat = NoiseStep(state->noiseSeed) * 0.07f * hatGate;

    // Slight sidechain from kick envelope.
    const float duck = 1.0f - state->kickEnv * 0.32f;
    float sample = kick + snare + (bass + lead + hat + blip) * duck;
    sample = std::tanh(sample * 1.25f);
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
    uniform int iStep;
    uniform float iStepPhase;

    float hash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }

    float sdBox(vec2 p, vec2 b) {
      vec2 d = abs(p) - b;
      return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
    }

    float stroke(vec2 p, vec2 c, vec2 b) {
      return 1.0 - smoothstep(0.0, 0.004, sdBox(p - c, b));
    }

    float glyphR(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.36, 0.0), vec2(0.10, 0.50));
      g += stroke(p, vec2(0.00, 0.40), vec2(0.24, 0.10));
      g += stroke(p, vec2(0.20, 0.12), vec2(0.10, 0.18));
      g += stroke(p, vec2(0.10, -0.20), vec2(0.08, 0.08));
      g += stroke(p, vec2(0.28, -0.34), vec2(0.08, 0.08));
      return clamp(g, 0.0, 1.0);
    }

    float glyphF(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.35, 0.0), vec2(0.10, 0.50));
      g += stroke(p, vec2(0.02, 0.40), vec2(0.30, 0.10));
      g += stroke(p, vec2(-0.02, 0.05), vec2(0.26, 0.09));
      return clamp(g, 0.0, 1.0);
    }

    float glyphD(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.35, 0.0), vec2(0.10, 0.50));
      g += stroke(p, vec2(0.03, 0.40), vec2(0.24, 0.10));
      g += stroke(p, vec2(0.03, -0.40), vec2(0.24, 0.10));
      g += stroke(p, vec2(0.27, 0.0), vec2(0.10, 0.30));
      return clamp(g, 0.0, 1.0);
    }

    float glyphS(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(0.00, 0.40), vec2(0.33, 0.10));
      g += stroke(p, vec2(-0.25, 0.16), vec2(0.10, 0.16));
      g += stroke(p, vec2(0.00, 0.00), vec2(0.30, 0.09));
      g += stroke(p, vec2(0.25, -0.16), vec2(0.10, 0.16));
      g += stroke(p, vec2(0.00, -0.40), vec2(0.33, 0.10));
      return clamp(g, 0.0, 1.0);
    }

    float glyphW(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.34, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(0.34, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(-0.10, -0.24), vec2(0.09, 0.20));
      g += stroke(p, vec2(0.10, -0.24), vec2(0.09, 0.20));
      return clamp(g, 0.0, 1.0);
    }

    float glyphA(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.24, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(0.24, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(0.00, 0.40), vec2(0.30, 0.10));
      g += stroke(p, vec2(0.00, 0.03), vec2(0.24, 0.09));
      return clamp(g, 0.0, 1.0);
    }

    float glyphH(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.30, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(0.30, 0.0), vec2(0.09, 0.50));
      g += stroke(p, vec2(0.00, 0.0), vec2(0.26, 0.09));
      return clamp(g, 0.0, 1.0);
    }

    float glyphE(vec2 p) {
      float g = 0.0;
      g += stroke(p, vec2(-0.33, 0.0), vec2(0.10, 0.50));
      g += stroke(p, vec2(0.02, 0.40), vec2(0.30, 0.10));
      g += stroke(p, vec2(-0.02, 0.00), vec2(0.24, 0.09));
      g += stroke(p, vec2(0.02, -0.40), vec2(0.30, 0.10));
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

    float drawLetter(vec2 uv, vec2 center, float scale, int letterId, float build, float blink, float wobble) {
      vec2 p = (uv - center) / scale;
      p.x += sin(p.y * 9.0 + iTime * 14.0) * wobble;
      float mask = glyphFromId(p, letterId);
      float sweep = smoothstep(-0.55, 0.55, p.y + (build * 1.1 - 0.55));
      float lit = mask * sweep;
      float glow = mask * 0.35;
      return lit * blink + glow;
    }

    void main() {
      vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
      float t = iTime;
      vec3 col = vec3(0.0);

      // Monochrome-ish cracktro background (less green).
      float scan = 0.88 + 0.12 * sin(gl_FragCoord.y * 1.7);
      float grid = 0.06 * sin(gl_FragCoord.x * 0.035 + t * 1.6);
      col += vec3(0.03, 0.035, 0.04) * scan + vec3(0.02, 0.025, 0.03) * grid;

      // Left/right vertical bars for cracktro framing.
      float side = step(0.92, abs(uv.x));
      col += vec3(0.32, 0.34, 0.38) * side;

      // Main logo "RFDS": each beat builds next letter, then loops.
      float baseX = -0.42;
      float spacing = 0.28;
      float scale = 0.22;
      for (int i = 0; i < 4; ++i) {
        float active = (i == iStep) ? 1.0 : 0.0;
        float built = 0.0;
        if (i < iStep) built = 1.0;
        if (i == iStep) built = iStepPhase;

        vec2 c = vec2(baseX + float(i) * spacing, 0.06);
        c.y += sin(t * 10.0 + float(i) * 1.7) * 0.015 * (0.3 + active * 0.7);
        float s = scale * (1.0 + active * 0.13 * (1.0 - iStepPhase));
        float blink = 0.65 + 0.35 * smoothstep(0.0, 1.0, iBeat + active * 0.3);
        float wobble = 0.01 + active * 0.035 * (1.0 - iStepPhase);

        float letter = drawLetter(uv, c, s, i, built, blink, wobble);
        vec3 baseCol = vec3(0.78, 0.82, 0.88);
        vec3 activeCol = vec3(0.65, 0.92, 1.0);
        col += mix(baseCol, activeCol, active) * letter;
      }

      // Blinking subtext "RFDS WAS HERE".
      float subBlink = step(0.48, fract(t * 2.4 + iBeat * 0.2));
      float tiny = 0.085;
      float ySub = -0.52;
      float x = -0.58;
      int seq[11];
      seq[0] = 0;  // R
      seq[1] = 1;  // F
      seq[2] = 2;  // D
      seq[3] = 3;  // S
      seq[4] = -1; // space
      seq[5] = 4;  // W
      seq[6] = 5;  // A
      seq[7] = 3;  // S
      seq[8] = -1; // space
      seq[9] = 6;  // H
      seq[10] = 7; // E

      for (int i = 0; i < 11; ++i) {
        if (seq[i] >= 0) {
          float l = drawLetter(uv, vec2(x, ySub), tiny, seq[i], 1.0, subBlink, 0.0);
          col += vec3(0.78, 0.84, 0.92) * l;
        }
        x += 0.11;
      }
      // Append "RE" to complete HERE.
      col += vec3(0.78, 0.84, 0.92) * drawLetter(uv, vec2(0.63, ySub), tiny, 0, 1.0, subBlink, 0.0);
      col += vec3(0.78, 0.84, 0.92) * drawLetter(uv, vec2(0.74, ySub), tiny, 7, 1.0, subBlink, 0.0);

      // Add tiny random sparkle bits.
      vec2 fp = floor(gl_FragCoord.xy * 0.25 + vec2(t * 14.0, 0.0));
      float spark = step(0.996, hash(fp));
      col += vec3(0.85, 0.95, 1.0) * spark * 0.25;

      // Vignette.
      float vignette = 1.0 - smoothstep(0.75, 1.35, length(uv));
      col *= 0.30 + 0.70 * vignette;
      col += vec3(0.08, 0.10, 0.12) * iBeat * 0.5;
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

  SDL_Window* window = SDL_CreateWindow("razor_rfds_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWidth, kHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if (!glContext) {
    std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_GL_SetSwapInterval(1);

  AudioState audioState;
  SDL_AudioSpec want{};
  want.freq = kSampleRate;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = SDLAudioCallback;
  want.userdata = &audioState;

  SDL_AudioSpec have{};
  SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio == 0) {
    std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    SDL_GL_DeleteContext(glContext);
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
  const GLint iStepPhaseLoc = glGetUniformLocation(program, "iStepPhase");

  bool running = true;
  bool fullscreen = false;
  const Uint64 start = SDL_GetPerformanceCounter();
  float shotAt = -1.0f;
  std::string shotPath;
  ParseShotEnv(shotAt, shotPath);
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
        if (event.key.keysym.sym == SDLK_f) {
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
    const float beat = audioState.beatPulse.load(std::memory_order_relaxed);
    const int step = audioState.letterStep.load(std::memory_order_relaxed);
    const float beatPos = timeSec * (kBpm / 60.0f);
    const float stepPhase = beatPos - std::floor(beatPos);
    audioState.beatPulse.store(beat * 0.93f, std::memory_order_relaxed);

    glUseProgram(program);
    glUniform2f(iResLoc, static_cast<float>(w), static_cast<float>(h));
    glUniform1f(iTimeLoc, timeSec);
    glUniform1f(iBeatLoc, beat);
    glUniform1i(iStepLoc, step);
    glUniform1f(iStepPhaseLoc, stepPhase);

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
  SDL_GL_DeleteContext(glContext);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
