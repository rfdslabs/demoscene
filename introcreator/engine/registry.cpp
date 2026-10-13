#include "registry.h"
#include <cstring>

namespace ti {

  // Meyers singleton so registration during static init is safe regardless of
  // TU initialization order.
  static EffectRegistry& reg() {
    static EffectRegistry r;
    return r;
  }

  const EffectRegistry& effects() { return reg(); }

  int register_effect(const EffectInfo* e) {
    EffectRegistry& r = reg();
    if (r.count < 32) r.items[r.count] = e;
    return r.count++;
  }

  const EffectInfo* EffectRegistry::find(const char* id) const {
    for (int i = 0; i < count; ++i)
      if (std::strcmp(items[i]->id, id) == 0) return items[i];
    return nullptr;
  }
}
