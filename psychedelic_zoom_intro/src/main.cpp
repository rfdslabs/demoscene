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
  float noiseSeed = 0.1234f;
  float nextBeat = 0.0f;
  int beat = 0;
  std::atomic<float> beatPulse{0.0f};
};

float Fract(float x) { return x - std::floor(x); }

float NoiseStep(float& seed) {
  seed = Fract(seed * 12.9898f + 78.233f);
  return Fract(std::sin(seed * 43758.5453f) * 43758.5453f) * 2.0f - 1.0f;
}

void AudioCallback(void* userdata, Uint8* stream, int len) {
  auto* st = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float bpm = 138.0f;
  const float beatDur = 60.0f / bpm;
  const std::array<float, 8> bassNotes = {55.0f, 55.0f, 65.41f, 55.0f, 73.42f, 65.41f, 55.0f, 49.0f};
  const std::array<float, 8> leadNotes = {220.0f, 246.94f, 293.66f, 329.63f, 293.66f, 246.94f, 220.0f, 196.0f};

  for (int i = 0; i < samples; ++i) {
    if (st->t >= st->nextBeat) {
      st->kickEnv = 1.0f;
      st->nextBeat += beatDur;
      st->beat++;
      st->beatPulse.store(1.0f, std::memory_order_relaxed);
    }

    const float bassFreq = bassNotes[st->beat % static_cast<int>(bassNotes.size())];
    const float leadFreq = leadNotes[(st->beat * 2) % static_cast<int>(leadNotes.size())];

    const float kickFreq = 44.0f + st->kickEnv * 170.0f;
    float kick = std::sin(2.0f * kPi * kickFreq * st->t) * st->kickEnv * 0.9f;
    st->kickEnv *= 0.996f;

    st->bassPhase += 2.0f * kPi * bassFreq * dt;
    if (st->bassPhase > 2.0f * kPi) st->bassPhase -= 2.0f * kPi;
    float bassSaw = (st->bassPhase / kPi) - 1.0f;
    bassSaw = std::tanh(bassSaw * 1.6f);
    const float bass = bassSaw * 0.32f;

    st->leadPhase += 2.0f * kPi * leadFreq * dt;
    if (st->leadPhase > 2.0f * kPi) st->leadPhase -= 2.0f * kPi;
    const float lead = std::sin(st->leadPhase + std::sin(st->bassPhase * 2.0f) * 0.7f) * 0.19f;

    const float hat = NoiseStep(st->noiseSeed) * ((st->beat % 2) ? 0.08f : 0.02f);
    const float duck = 1.0f - st->kickEnv * 0.28f;
    float s = kick + (bass + lead + hat) * duck;
    s = std::tanh(s * 1.2f);
    out[i] = s * 0.72f;
    st->t += dt;
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

    float hash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
    }

    vec3 palette(float t) {
      vec3 a = vec3(0.5, 0.5, 0.5);
      vec3 b = vec3(0.5, 0.5, 0.5);
      vec3 c = vec3(1.0, 1.0, 1.0);
      vec3 d = vec3(0.00, 0.12, 0.20);
      return a + b * cos(6.28318 * (c * t + d));
    }

    void main() {
      vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
      float t = iTime;

      // Infinite zoom pulse.
      float cycle = fract(t * 0.16);
      float zoom = pow(2.0, cycle * 8.0);
      uv /= zoom;

      // Kaleidoscope fold.
      float a = atan(uv.y, uv.x);
      float r = length(uv);
      float sectors = 8.0 + 4.0 * sin(t * 0.3);
      a = abs(mod(a, 6.28318 / sectors) - 3.14159 / sectors);
      uv = vec2(cos(a), sin(a)) * r;

      // Warp.
      uv += 0.22 * vec2(sin(uv.y * 7.0 + t * 1.8), cos(uv.x * 6.0 - t * 1.7));

      vec2 z = uv;
      float m = 0.0;
      float glow = 0.0;
      for (int i = 0; i < 26; ++i) {
        z = abs(z) / dot(z, z) - 0.83;
        float l = length(z);
        glow += exp(-6.0 * abs(l - 1.2));
        m += l;
      }

      float bands = sin(m * 0.08 + t * 2.0) * 0.5 + 0.5;
      float rings = sin((r * zoom) * 55.0 - t * 10.0) * 0.5 + 0.5;
      float noise = hash(floor(gl_FragCoord.xy * 0.75 + vec2(t * 80.0, -t * 40.0)));

      vec3 col = palette(bands + rings * 0.35 + t * 0.07);
      col += palette(bands * 0.8 + 0.25) * glow * 0.045;
      col *= 0.75 + 0.35 * rings;
      col += vec3(noise) * 0.05;

      // Strobe bursts on cycle edges.
      float flash = smoothstep(0.95, 1.0, cycle) + (1.0 - smoothstep(0.0, 0.05, cycle));
      col += vec3(1.0, 0.7, 0.4) * flash * (0.20 + iBeat * 0.55);

      // Gentle vignette.
      col *= 1.0 - smoothstep(0.95, 1.45, length((gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y));
      col += vec3(0.08, 0.03, 0.10) * iBeat * 0.35;

      gl_FragColor = vec4(pow(max(col, 0.0), vec3(0.9)), 1.0);
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

  SDL_Window* window = SDL_CreateWindow("psychedelic_zoom_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
  SDL_AudioSpec want{};
  want.freq = kSampleRate;
  want.format = AUDIO_F32SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = AudioCallback;
  want.userdata = &audioState;
  SDL_AudioSpec have{};
  SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
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

  const Uint64 start = SDL_GetPerformanceCounter();
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

    int w = 0;
    int h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    const Uint64 now = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float timeSec = static_cast<float>((now - start) / freq);
    const float beat = audioState.beatPulse.load(std::memory_order_relaxed);
    audioState.beatPulse.store(beat * 0.93f, std::memory_order_relaxed);

    glUseProgram(program);
    glUniform2f(iResLoc, static_cast<float>(w), static_cast<float>(h));
    glUniform1f(iTimeLoc, timeSec);
    glUniform1f(iBeatLoc, beat);

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
