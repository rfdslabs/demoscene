#pragma once
#include "effect.h"
namespace ti {
  struct EffectRegistry {
    const EffectInfo* items[32];
    int count = 0;
    const EffectInfo* find(const char* id) const;
  };

  const EffectRegistry& effects();
  int register_effect(const EffectInfo* e);   // returns index; used by TI_REGISTER_EFFECT
}

// Register an EffectInfo at static-init time. `sym` is a file-scope EffectInfo.
#define TI_REGISTER_EFFECT(sym) \
  static int _ti_reg_##sym = ti::register_effect(&sym)
