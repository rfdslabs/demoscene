#pragma once
// TinyIntro — zero-dependency test harness (no external framework, per project constraint).
#include <cstdio>
#include <cmath>

inline int& g_fail()  { static int f = 0; return f; }
inline int& g_count() { static int c = 0; return c; }

#define CHECK(cond) do { g_count()++; if(!(cond)) { \
  std::printf("  FAIL %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); g_fail()++; } } while(0)

#define CHECK_EQ(a,b) do { g_count()++; auto _a=(a); auto _b=(b); if(!((_a)==(_b))) { \
  std::printf("  FAIL %s:%d  CHECK_EQ(%s,%s)\n", __FILE__, __LINE__, #a, #b); g_fail()++; } } while(0)

#define CHECK_NEAR(a,b,eps) do { g_count()++; double _d=std::fabs((double)(a)-(double)(b)); \
  if(_d>(eps)) { std::printf("  FAIL %s:%d  CHECK_NEAR(%s,%s) diff=%g\n", __FILE__,__LINE__,#a,#b,_d); g_fail()++; } } while(0)

#define RUN_TEST(fn) do { std::printf("[ RUN ] %s\n", #fn); fn(); } while(0)

inline int tinyintro_tests_failed() { return g_fail(); }
