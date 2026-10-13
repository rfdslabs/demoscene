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
  float padPhaseA = 0.0f;
  float padPhaseB = 0.0f;
  float padPhaseC = 0.0f;
  float dronePhase = 0.0f;
  float chimeEnv = 0.0f;
  float chimePhase = 0.0f;
  float chimeFreq = 440.0f;
  float nextBeat = 0.0f;
  int beat = 0;
  std::atomic<float> beatPulse{0.0f};
};

void AudioCallback(void* userdata, Uint8* stream, int len) {
  auto* st = static_cast<AudioState*>(userdata);
  auto* out = reinterpret_cast<float*>(stream);
  const int samples = len / static_cast<int>(sizeof(float));
  const float dt = 1.0f / static_cast<float>(kSampleRate);
  const float bpm = 78.0f;
  const float beatDur = 60.0f / bpm;
  const std::array<float, 8> root = {196.0f, 220.0f, 246.94f, 293.66f, 246.94f, 220.0f, 196.0f, 174.61f};

  for (int i = 0; i < samples; ++i) {
    if (st->t >= st->nextBeat) {
      const int idx = st->beat % static_cast<int>(root.size());
      const float r = root[idx];
      st->chimeFreq = r * (idx % 2 ? 2.0f : 3.0f);
      st->chimeEnv = 1.0f;
      st->nextBeat += beatDur;
      st->beat++;
      st->beatPulse.store(1.0f, std::memory_order_relaxed);
    }

    const int idx = st->beat % static_cast<int>(root.size());
    const float r = root[idx];
    st->padPhaseA += 2.0f * kPi * r * dt;
    st->padPhaseB += 2.0f * kPi * (r * 1.5f) * dt;
    st->padPhaseC += 2.0f * kPi * (r * 2.0f) * dt;
    st->dronePhase += 2.0f * kPi * 49.0f * dt;
    if (st->padPhaseA > 2.0f * kPi) st->padPhaseA -= 2.0f * kPi;
    if (st->padPhaseB > 2.0f * kPi) st->padPhaseB -= 2.0f * kPi;
    if (st->padPhaseC > 2.0f * kPi) st->padPhaseC -= 2.0f * kPi;
    if (st->dronePhase > 2.0f * kPi) st->dronePhase -= 2.0f * kPi;

    const float pad =
        (std::sin(st->padPhaseA) * 0.52f + std::sin(st->padPhaseB) * 0.30f + std::sin(st->padPhaseC) * 0.18f) *
        0.27f;
    const float drone = std::sin(st->dronePhase) * 0.14f;

    st->chimePhase += 2.0f * kPi * st->chimeFreq * dt;
    if (st->chimePhase > 2.0f * kPi) st->chimePhase -= 2.0f * kPi;
    const float chime = std::sin(st->chimePhase) * st->chimeEnv * 0.12f;
    st->chimeEnv *= 0.995f;

    float s = pad + drone + chime;
    s = std::tanh(s * 1.05f);
    out[i] = s * 0.66f;
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
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }

    float noise(vec2 p) {
      vec2 i = floor(p);
      vec2 f = fract(p);
      vec2 u = f * f * (3.0 - 2.0 * f);
      return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
                 mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
    }

    float fbm(vec2 p) {
      float v = 0.0;
      float a = 0.5;
      for (int i = 0; i < 5; ++i) {
        v += noise(p) * a;
        p = p * 2.1 + vec2(13.4, 7.1);
        a *= 0.5;
      }
      return v;
    }

    float terrainHeight(vec2 p) {
      float island = exp(-dot(p, p) * 0.06);
      float detail = fbm(p * 0.35) * 0.55 + fbm(p * 1.2) * 0.15;
      return island * 2.6 + detail * island * 1.4 - 0.35;
    }

    vec3 terrainNormal(vec2 p) {
      float e = 0.02;
      float h = terrainHeight(p);
      float hx = terrainHeight(p + vec2(e, 0.0));
      float hz = terrainHeight(p + vec2(0.0, e));
      return normalize(vec3(h - hx, e, h - hz));
    }

    vec3 waterNormal(vec2 p, float t) {
      float e = 0.04;
      float h = sin(p.x * 0.75 + t * 1.3) * 0.03 + cos(p.y * 0.95 - t * 1.1) * 0.02;
      float hx = sin((p.x + e) * 0.75 + t * 1.3) * 0.03 + cos(p.y * 0.95 - t * 1.1) * 0.02;
      float hz = sin(p.x * 0.75 + t * 1.3) * 0.03 + cos((p.y + e) * 0.95 - t * 1.1) * 0.02;
      return normalize(vec3(h - hx, e, h - hz));
    }

    vec3 skyColor(vec3 rd, vec3 sunDir) {
      float up = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
      vec3 sky = mix(vec3(0.15, 0.24, 0.35), vec3(0.58, 0.75, 0.93), up);
      float sun = pow(max(dot(rd, sunDir), 0.0), 260.0);
      sky += vec3(1.0, 0.85, 0.62) * sun;
      return sky;
    }

    void main() {
      vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
      float t = iTime;

      // Cinematic camera orbit + subtle zoom.
      float camDist = 6.0 + sin(t * 0.12) * 0.8;
      vec3 ro = vec3(cos(t * 0.08) * camDist, 1.9 + sin(t * 0.23) * 0.15, sin(t * 0.08) * camDist);
      vec3 ta = vec3(0.0, 0.45, 0.0);

      vec3 ww = normalize(ta - ro);
      vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
      vec3 vv = normalize(cross(uu, ww));
      float lens = 1.75;
      vec3 rd = normalize(uu * uv.x + vv * uv.y + ww * lens);

      vec3 sunDir = normalize(vec3(0.55, 0.62, 0.25));
      vec3 col = skyColor(rd, sunDir);

      float tTerrain = 1e9;
      float tWater = 1e9;

      // Raymarch terrain.
      float d = 0.0;
      for (int i = 0; i < 120; ++i) {
        vec3 p = ro + rd * d;
        float h = p.y - terrainHeight(p.xz);
        if (h < 0.01) {
          tTerrain = d;
          break;
        }
        d += clamp(h * 0.65, 0.02, 0.2);
        if (d > 30.0) break;
      }

      // Water plane y=0.
      if (abs(rd.y) > 0.0001) {
        float tw = -ro.y / rd.y;
        if (tw > 0.0) tWater = tw;
      }

      bool hitTerrain = tTerrain < tWater && tTerrain < 30.0;
      bool hitWater = tWater < tTerrain && tWater < 30.0;

      if (hitTerrain) {
        vec3 p = ro + rd * tTerrain;
        vec3 n = terrainNormal(p.xz);
        float nDotL = max(dot(n, sunDir), 0.0);

        float h = terrainHeight(p.xz);
        float slope = 1.0 - n.y;
        vec3 sand = vec3(0.80, 0.72, 0.58);
        vec3 grass = vec3(0.22, 0.46, 0.20);
        vec3 rock = vec3(0.36, 0.34, 0.32);

        float beach = smoothstep(0.00, 0.35, h);
        vec3 albedo = mix(sand, grass, beach);
        albedo = mix(albedo, rock, smoothstep(0.28, 0.65, slope));

        float ao = clamp(0.55 + n.y * 0.45, 0.0, 1.0);
        vec3 lit = albedo * (0.24 + nDotL * 0.95) * ao;

        // Tiny vegetation tint variation.
        float veg = fbm(p.xz * 3.0 + t * 0.02);
        lit *= 0.94 + 0.12 * veg;

        // Specular glints near shoreline.
        vec3 hdir = normalize(sunDir - rd);
        float spec = pow(max(dot(n, hdir), 0.0), 36.0) * smoothstep(0.0, 0.3, 0.3 - h);
        lit += vec3(1.0, 0.9, 0.7) * spec * 0.35;

        col = lit;
      } else if (hitWater) {
        vec3 p = ro + rd * tWater;
        vec3 wn = waterNormal(p.xz, t);
        float fres = pow(1.0 - max(dot(-rd, wn), 0.0), 4.0);
        float nDotL = max(dot(wn, sunDir), 0.0);
        vec3 refl = skyColor(reflect(rd, wn), sunDir);
        vec3 deep = vec3(0.04, 0.18, 0.28);
        vec3 water = mix(deep, refl, fres * 0.9);
        water += vec3(1.0, 0.9, 0.7) * pow(nDotL, 80.0) * 0.55;
        col = water;
      }

      // Atmospheric fog + beat-linked tiny exposure lift.
      float dist = min(min(tTerrain, tWater), 30.0);
      float fog = 1.0 - exp(-dist * 0.07);
      col = mix(col, skyColor(rd, sunDir), fog);
      col *= 0.98 + iBeat * 0.03;

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

  SDL_Window* window = SDL_CreateWindow("realism_island_intro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

  SDL_GL_SetSwapInterval(1);
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
    audioState.beatPulse.store(beat * 0.94f, std::memory_order_relaxed);

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
