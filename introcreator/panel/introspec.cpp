#include "introspec.h"
#include "registry.h"
#include "surface.h"
#include <sstream>
#include <fstream>

namespace ti {

  double IntroSpec::total_beats() const {
    double end = 0.0;
    for (const auto& sc : scenes) {
      double e = sc.start_beat + sc.length_beats;
      if (e > end) end = e;
    }
    return end;
  }

  // .tro text format — one directive per line:
  //   title <rest of line>
  //   bpm <number>
  //   scroll <rest of line>
  //   scene <id> <start_beat> <len_beats> <p0> <p1> ... <p7>
  std::string IntroSpec::to_string() const {
    std::ostringstream o;
    o << "title " << title << "\n";
    o << "bpm " << bpm << "\n";
    o << "scroll " << scroller_text << "\n";
    for (const auto& sc : scenes) {
      o << "scene " << sc.effect_id << " " << sc.start_beat << " " << sc.length_beats;
      for (int i = 0; i < 8; ++i) o << " " << sc.p[i];
      o << "\n";
    }
    return o.str();
  }

  IntroSpec IntroSpec::from_string(const std::string& text) {
    IntroSpec spec;
    spec.scenes.clear();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      std::istringstream ls(line);
      std::string key; ls >> key;
      if (key == "title") { std::string rest; std::getline(ls, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1); spec.title = rest; }
      else if (key == "bpm") { ls >> spec.bpm; }
      else if (key == "scroll") { std::string rest; std::getline(ls, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1); spec.scroller_text = rest; }
      else if (key == "scene") {
        SpecScene sc;
        ls >> sc.effect_id >> sc.start_beat >> sc.length_beats;
        for (int i = 0; i < 8; ++i) ls >> sc.p[i];
        spec.scenes.push_back(sc);
      }
    }
    return spec;
  }

  bool IntroSpec::save(const char* path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << to_string();
    return true;
  }

  IntroSpec IntroSpec::load(const char* path) {
    std::ifstream f(path);
    if (!f) return IntroSpec{};
    std::ostringstream ss; ss << f.rdbuf();
    return from_string(ss.str());
  }

  IntroSpec IntroSpec::demo() {
    IntroSpec s;
    s.title = "TinyIntro Demo";
    s.bpm = 125.0;
    s.scroller_text =
      "TINYINTRO PRESENTS ... ANOTHER USELESS PIECE OF SOFTWARE :) ... "
      "GREETZ TO EVERYONE STILL WRITING C++ IN 2026 ... "
      "SIX CLASSIC EFFECTS, ZERO EXTERNAL LIBS, ONE TINY BINARY ... WRAP !     ";
    auto add = [&](const char* id, double start, double len, std::initializer_list<float> ps) {
      SpecScene sc; sc.effect_id = id; sc.start_beat = start; sc.length_beats = len;
      int i = 0; for (float v : ps) { if (i < 8) sc.p[i++] = v; }
      s.scenes.push_back(sc);
    };
    add("starfield",  0,  16, {0.6f});
    add("logo",       16, 16, {1});
    add("plasma",     32, 16, {0, 0});
    add("copperbars", 48, 16, {0});
    add("vector3d",   64, 16, {0, 0});
    add("rotozoom",   80, 16, {0});
    add("tunnel",     96, 16, {1});
    // Scroller runs across the whole thing on top (handled by play()).
    return s;
  }

  // ---- shared player -------------------------------------------------------
  // Renders the background scene for `time`, then always overlays the scroller
  // using the global scroller_text. This exact logic is what the baked binary
  // reproduces, so preview == release.
  void play(const IntroSpec& spec, Surface& s, double time) {
    double beat = time * spec.bpm / 60.0;

    const SpecScene* cur = nullptr;
    for (const auto& sc : spec.scenes) {
      if (beat >= sc.start_beat && beat < sc.start_beat + sc.length_beats) { cur = &sc; break; }
    }

    if (cur) {
      const EffectInfo* e = effects().find(cur->effect_id.c_str());
      if (e) {
        Ctx c;
        c.time = time;
        c.beat = beat;
        c.frame = (int)(time * 60.0);
        Params p;
        for (int i = 0; i < 8; ++i) p.p[i] = cur->p[i];
        e->render(s, c, p);
      } else {
        s.clear(0);
      }
    } else {
      s.clear(0);
    }

    // Scroller overlay for the whole intro.
    const EffectInfo* sc_eff = effects().find("scroller");
    if (sc_eff) {
      Ctx c; c.time = time; c.beat = beat; c.frame = (int)(time * 60.0);
      Params p;
      p.text = spec.scroller_text.c_str();
      p.p[0] = H - 40;   // base_y
      p.p[1] = 18;       // amp
      p.p[2] = 255;      // color index
      p.p[3] = 100;      // speed
      s.set_pal(255, 255, 255, 255);
      sc_eff->render(s, c, p);
    }
  }
}
