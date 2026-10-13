// TINYINTRO CREATOR — the old-school control panel.
//
// The panel is drawn with the very engine it configures: an 8-bit indexed
// Surface, the pixel font, palette colours. The live preview in the top-right is
// the real intro rendered through the SHARED play() path — so what you see is
// exactly what the baked binary produces. Keyboard driven, BBS style.
#include "platform.h"
#include "introspec.h"
#include "registry.h"
#include "surface.h"
#include "font.h"
#include "tables.h"
#include "synth.h"
#include "rng.h"
#include "bake.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

namespace {

  // macOS virtual key codes we use beyond the ones in platform.h.
  enum { K_A=0, K_S=1, K_D=2, K_Z=6, K_R=15, K_L=37, K_B=11,
         K_MINUS=27, K_EQUAL=24, K_LBRACK=33, K_RBRACK=30 };

  // UI palette indices.
  enum { C_BG=0, C_BAR=1, C_CYAN=2, C_WHITE=3, C_HI=4, C_GRAY=5, C_GREEN=6, C_RED=7 };

  ti::Surface g_ui;
  ti::Surface g_prev;                       // intro rendered here for the preview
  unsigned int g_prev_argb[ti::W * ti::H];  // its expansion
  ti::Synth*  g_synth = nullptr;            // set in main() once audio starts

  void fill_rect(ti::Surface& s, int x0, int y0, int w, int h, unsigned char c) {
    for (int y = y0; y < y0 + h; ++y)
      for (int x = x0; x < x0 + w; ++x)
        s.put(x, y, c);
  }

  void frame_rect(ti::Surface& s, int x0, int y0, int w, int h, unsigned char c) {
    for (int x = x0; x < x0 + w; ++x) { s.put(x, y0, c); s.put(x, y0 + h - 1, c); }
    for (int y = y0; y < y0 + h; ++y) { s.put(x0, y, c); s.put(x0 + w - 1, y, c); }
  }

  struct PanelApp : plat::App {
    ti::IntroSpec spec;
    std::vector<const char*> fx;      // effect ids from the registry, for cycling
    int  sel_scene = 0;
    int  sel_param = 0;
    bool playing = true;
    double preview_time = 0.0;
    double last_time = -1.0;
    unsigned rnd_seed = 1;
    std::string status = "READY  -  CLICK OR USE KEYS";

    // Clickable hot regions, rebuilt every frame during draw.
    enum { A_SCENE, A_PARAM, A_FXP, A_FXN, A_DEC, A_INC,
           A_PLAY, A_ADD, A_DEL, A_RND, A_SAVE, A_LOAD, A_BAKE };
    struct Hot { int x, y, w, h, act, arg; };
    std::vector<Hot> hots;

    PanelApp() {
      tt::init_tables();
      spec = ti::IntroSpec::demo();
      const ti::EffectRegistry& r = ti::effects();
      for (int i = 0; i < r.count; ++i) fx.push_back(r.items[i]->id);
    }

    const ti::EffectInfo* sel_effect() const {
      if (spec.scenes.empty()) return nullptr;
      return ti::effects().find(spec.scenes[sel_scene].effect_id.c_str());
    }
    int param_count(const ti::EffectInfo* e) const {
      if (!e) return 0;
      int n = 0; while (n < 8 && e->param_names[n]) n++;
      return n;
    }

    void blip(int k) { if (g_synth) g_synth->blip(k); }

    int draw_btn(int x, int y, const char* label, int act, int arg) {
      int w = (int)std::strlen(label) * 8 + 6;
      fill_rect(g_ui, x, y - 1, w - 2, 9, C_BAR);
      draw_text_upper(x + 2, y, label, C_WHITE);
      hots.push_back({x, y - 1, w - 2, 9, act, arg});
      return w;
    }

    void do_action(int act, int arg) {
      switch (act) {
        case A_SCENE: sel_scene = arg; sel_param = 0; blip(1); break;
        case A_PARAM: sel_param = arg; blip(1); break;
        case A_FXP:   cycle_effect(-1); blip(1); break;
        case A_FXN:   cycle_effect(+1); blip(1); break;
        case A_DEC:   sel_param = arg; adjust_param(-1); blip(1); break;
        case A_INC:   sel_param = arg; adjust_param(+1); blip(1); break;
        case A_PLAY:  playing = !playing; status = playing ? "PLAYING" : "PAUSED"; blip(2); break;
        case A_ADD:   add_scene(); blip(2); break;
        case A_DEL:   del_scene(); blip(2); break;
        case A_RND:   randomize_scene(); blip(2); break;
        case A_SAVE:  do_save(); blip(2); break;
        case A_LOAD:  do_load(); blip(2); break;
        case A_BAKE:  do_bake(); blip(2); break;
      }
    }

    void mouse(int x, int y, bool down) override {
      if (!down) return;
      for (const Hot& h : hots)
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
          do_action(h.act, h.arg);
          return;
        }
    }

    // ---- input ----
    void key(int code, bool down) override {
      if (!down) return;
      int nsc = (int)spec.scenes.size();
      switch (code) {
        case plat::KEY_UP:    if (sel_scene > 0) sel_scene--; sel_param = 0; blip(1); break;
        case plat::KEY_DOWN:  if (sel_scene < nsc - 1) sel_scene++; sel_param = 0; blip(1); break;
        case plat::KEY_LEFT:  cycle_effect(-1); blip(1); break;
        case plat::KEY_RIGHT: cycle_effect(+1); blip(1); break;
        case K_LBRACK: { int n = param_count(sel_effect()); if (n) sel_param = (sel_param + n - 1) % n; } blip(1); break;
        case K_RBRACK: { int n = param_count(sel_effect()); if (n) sel_param = (sel_param + 1) % n; } blip(1); break;
        case K_MINUS:  adjust_param(-1); blip(1); break;
        case K_EQUAL:  adjust_param(+1); blip(1); break;
        case plat::KEY_SPACE: playing = !playing; status = playing ? "PLAYING" : "PAUSED"; blip(2); break;
        case K_R: preview_time = 0.0; status = "PREVIEW RESTARTED"; blip(2); break;
        case K_A: add_scene(); blip(2); break;
        case K_D: del_scene(); blip(2); break;
        case K_Z: randomize_scene(); blip(2); break;
        case K_S: do_save(); blip(2); break;
        case K_L: do_load(); blip(2); break;
        case K_B: do_bake(); blip(2); break;
        default: break;
      }
    }

    void cycle_effect(int dir) {
      if (spec.scenes.empty() || fx.empty()) return;
      const std::string& cur = spec.scenes[sel_scene].effect_id;
      int idx = 0;
      for (int i = 0; i < (int)fx.size(); ++i) if (cur == fx[i]) { idx = i; break; }
      idx = (idx + dir + (int)fx.size()) % (int)fx.size();
      spec.scenes[sel_scene].effect_id = fx[idx];
      sel_param = 0;
      status = std::string("EFFECT -> ") + fx[idx];
    }

    void adjust_param(int dir) {
      if (spec.scenes.empty()) return;
      const ti::EffectInfo* e = sel_effect();
      int n = param_count(e);
      if (n == 0) return;
      float& v = spec.scenes[sel_scene].p[sel_param];
      v += dir * 1.0f;
      char buf[64];
      std::snprintf(buf, sizeof buf, "%s = %.1f", e->param_names[sel_param], v);
      status = buf;
    }

    void add_scene() {
      ti::SpecScene sc;
      sc.effect_id = fx.empty() ? "plasma" : fx[0];
      double at = spec.total_beats();
      sc.start_beat = at; sc.length_beats = 16;
      spec.scenes.push_back(sc);
      sel_scene = (int)spec.scenes.size() - 1;
      status = "SCENE ADDED";
    }
    void del_scene() {
      if (spec.scenes.empty()) return;
      spec.scenes.erase(spec.scenes.begin() + sel_scene);
      if (sel_scene >= (int)spec.scenes.size()) sel_scene = (int)spec.scenes.size() - 1;
      if (sel_scene < 0) sel_scene = 0;
      status = "SCENE DELETED";
    }
    void randomize_scene() {
      if (spec.scenes.empty()) return;
      const ti::EffectInfo* e = sel_effect();
      int n = param_count(e);
      if (n == 0) { status = "NO PARAMS TO RANDOMIZE"; return; }
      ti::Rng rng(rnd_seed++ ^ (unsigned)(preview_time * 44100.0) ^ 0x9e3779b9u);
      for (int i = 0; i < n; ++i)
        spec.scenes[sel_scene].p[i] = (float)(rng.range(19) - 6);  // -6..12
      status = "RANDOMIZED PARAMS";
    }
    void do_save() {
      status = spec.save("demos/out.tro") ? "SAVED demos/out.tro" : "SAVE FAILED";
    }
    void do_load() {
      ti::IntroSpec s = ti::IntroSpec::load("demos/out.tro");
      if (!s.scenes.empty()) { spec = s; sel_scene = 0; status = "LOADED demos/out.tro"; }
      else status = "LOAD FAILED (no demos/out.tro)";
    }
    void do_bake() {
      std::string log; long bytes = 0;
      status = "BAKING...";
      bool ok = ti::bake(spec, "generated", &log, &bytes);
      char buf[96];
      if (ok) std::snprintf(buf, sizeof buf, "BAKED generated/intro  %ld bytes", bytes);
      else    std::snprintf(buf, sizeof buf, "BAKE FAILED: %.60s", log.c_str());
      status = buf;
    }

    // ---- draw ----
    void draw_text_upper(int x, int y, const char* s, unsigned char c) {
      // font is uppercase; draw as-is (font maps lowercase to uppercase anyway).
      ti::font::draw(g_ui, x, y, s, c);
    }

    void draw_wrapped_secret(int x, int y, int cols, const char* text, unsigned char c) {
      // Respect embedded newlines, wrap long lines to `cols` chars.
      int row = 0;
      std::string line;
      auto flush = [&](const std::string& ln) {
        draw_text_upper(x, y + row * 8, ln.c_str(), c);
        row++;
      };
      for (const char* p = text; ; ++p) {
        if (*p == '\n' || *p == '\0') {
          // wrap the accumulated line
          while ((int)line.size() > cols) { flush(line.substr(0, cols)); line = line.substr(cols); }
          flush(line);
          line.clear();
          if (*p == '\0') break;
        } else {
          line.push_back(*p);
        }
      }
    }

    void frame(double time, unsigned int* argb) override {
      if (last_time < 0) last_time = time;
      double dt = time - last_time; last_time = time;
      if (playing) preview_time += dt;
      double total_s = spec.total_beats() * 60.0 / spec.bpm;
      if (total_s > 0 && preview_time > total_s) preview_time = std::fmod(preview_time, total_s);

      // UI palette.
      g_ui.set_pal(C_BG, 12, 12, 24);
      g_ui.set_pal(C_BAR, 40, 30, 90);
      g_ui.set_pal(C_CYAN, 80, 220, 255);
      g_ui.set_pal(C_WHITE, 235, 235, 245);
      g_ui.set_pal(C_HI, 255, 220, 60);
      g_ui.set_pal(C_GRAY, 120, 120, 150);
      g_ui.set_pal(C_GREEN, 90, 255, 120);
      g_ui.set_pal(C_RED, 255, 90, 90);
      g_ui.clear(C_BG);
      hots.clear();

      // Title bar.
      fill_rect(g_ui, 0, 0, ti::W, 10, C_BAR);
      draw_text_upper(4, 1, "TINYINTRO CREATOR", C_HI);
      draw_text_upper(ti::W - 9 * 8, 1, "ESC=QUIT", C_CYAN);

      // Scene list (left).
      draw_text_upper(4, 14, "SCENES", C_CYAN);
      int y = 24;
      for (int i = 0; i < (int)spec.scenes.size(); ++i) {
        const ti::SpecScene& sc = spec.scenes[i];
        bool selrow = (i == sel_scene);
        char buf[40];
        std::snprintf(buf, sizeof buf, "%c%d %-10.10s %d-%d",
                      selrow ? '>' : ' ', i, sc.effect_id.c_str(),
                      (int)sc.start_beat, (int)(sc.start_beat + sc.length_beats));
        draw_text_upper(4, y, buf, selrow ? C_HI : C_WHITE);
        hots.push_back({4, y, 140, 9, A_SCENE, i});
        if (selrow) {                       // clickable effect changer
          draw_btn(146, y, "<", A_FXP, 0);
          draw_btn(160, y, ">", A_FXN, 0);
        }
        y += 9;
      }

      // Params of the selected scene.
      const ti::EffectInfo* e = sel_effect();
      y += 6;
      draw_text_upper(4, y, "PARAMS", C_CYAN); y += 10;
      int n = param_count(e);
      if (n == 0) { draw_text_upper(8, y, "(none)", C_GRAY); }
      for (int i = 0; i < n; ++i) {
        char buf[40];
        std::snprintf(buf, sizeof buf, "%c %-8.8s %6.1f",
                      i == sel_param ? '>' : ' ', e->param_names[i], spec.scenes[sel_scene].p[i]);
        draw_text_upper(8, y, buf, i == sel_param ? C_HI : C_WHITE);
        hots.push_back({8, y, 138, 9, A_PARAM, i});
        draw_btn(146, y, "-", A_DEC, i);
        draw_btn(160, y, "+", A_INC, i);
        y += 9;
      }

      // Preview box (top-right). Border in UI; contents composited after expand.
      const int px = 178, py = 24, pw = 138, ph = 88;
      frame_rect(g_ui, px - 1, py - 1, pw + 2, ph + 2, C_CYAN);
      draw_text_upper(px, py - 11, playing ? "LIVE PREVIEW" : "PREVIEW (PAUSED)", C_GREEN);
      hots.push_back({px, py, pw, ph, A_PLAY, 0});   // click preview to play/pause

      // Secret / explanation box (mid-right, below preview).
      draw_text_upper(px, py + ph + 4, "THE SECRET", C_CYAN);
      if (e && e->secret)
        draw_wrapped_secret(px, py + ph + 14, 17, e->secret, C_GRAY);

      // Status + clickable button bar (bottom).
      fill_rect(g_ui, 0, ti::H - 20, ti::W, 20, C_BG);
      draw_text_upper(4, ti::H - 18, status.c_str(), C_GREEN);
      int bx = 2, by = ti::H - 9;
      bx += draw_btn(bx, by, playing ? "PAUSE" : "PLAY", A_PLAY, 0) + 1;
      bx += draw_btn(bx, by, "ADD",  A_ADD,  0) + 1;
      bx += draw_btn(bx, by, "DEL",  A_DEL,  0) + 1;
      bx += draw_btn(bx, by, "RND",  A_RND,  0) + 1;
      bx += draw_btn(bx, by, "SAVE", A_SAVE, 0) + 1;
      bx += draw_btn(bx, by, "LOAD", A_LOAD, 0) + 1;
      bx += draw_btn(bx, by, "BAKE", A_BAKE, 0) + 1;

      // Expand UI to the output.
      g_ui.expand(argb);

      // Render the intro through the shared player and blit it, downscaled,
      // into the preview box. Separate surface/palette avoids clashing with UI.
      ti::play(spec, g_prev, preview_time);
      g_prev.expand(g_prev_argb);
      for (int ty = 0; ty < ph; ++ty) {
        int sy = ty * ti::H / ph;
        for (int tx = 0; tx < pw; ++tx) {
          int sx = tx * ti::W / pw;
          argb[(py + ty) * ti::W + (px + tx)] = g_prev_argb[sy * ti::W + sx];
        }
      }
    }
  };
}

static void dump_bmp(const char* path, const unsigned int* argb, int w, int h) {
  std::FILE* f = std::fopen(path, "wb");
  if (!f) return;
  int row = (w * 3 + 3) & ~3, size = 54 + row * h;
  unsigned char hd[54] = {0};
  hd[0]='B'; hd[1]='M'; *(int*)&hd[2]=size; *(int*)&hd[10]=54; *(int*)&hd[14]=40;
  *(int*)&hd[18]=w; *(int*)&hd[22]=h; *(short*)&hd[26]=1; *(short*)&hd[28]=24;
  std::fwrite(hd,1,54,f);
  std::vector<unsigned char> line(row, 0);
  for (int y=h-1;y>=0;--y){ for(int x=0;x<w;++x){ unsigned int p=argb[y*w+x];
      line[x*3]=p&0xFF; line[x*3+1]=(p>>8)&0xFF; line[x*3+2]=(p>>16)&0xFF; }
    std::fwrite(line.data(),1,row,f); }
  std::fclose(f);
}

int main() {
  static PanelApp app;
  // Headless one-frame dump for verification: TINYINTRO_DUMP=path ./tinyintro
  if (const char* out = std::getenv("TINYINTRO_DUMP")) {
    static unsigned int argb[ti::W * ti::H];
    app.playing = false;
    app.frame(2.5, argb);
    dump_bmp(out, argb, ti::W, ti::H);
    std::printf("dumped panel frame to %s\n", out);
    // Self-check the mouse pipeline: click scene row 2, expect selection to move.
    app.frame(2.5, argb);                 // rebuild hot regions
    app.mouse(10, 24 + 2 * 9, true);      // click the 3rd scene row
    std::printf("mouse click test: sel_scene=%d (expect 2)\n", app.sel_scene);
    return 0;
  }
  static ti::Synth synth;
  g_synth = &synth;
  plat::audio_start(&synth);      // music plays while you design; UI blips on input
  return plat::run(app, "TinyIntro Creator", 3);
}
