#include "test.h"
#include "tables.h"
#include <cmath>

static void test_isin_matches_libm() {
  tt::init_tables();
  for (int i = 0; i < tt::SIN_SIZE; i += 37) {
    double rad = (double)i / tt::SIN_SIZE * 2.0 * M_PI;
    double want = std::sin(rad) * tt::SIN_ONE;
    CHECK_NEAR(tt::isin(i), want, 8.0);
  }
  CHECK_EQ(tt::isin(0), 0);
  CHECK_EQ(tt::icos(0), tt::SIN_ONE);
  // wrap-around masks cleanly
  CHECK_EQ(tt::isin(tt::SIN_SIZE), tt::isin(0));
}

static void test_fsin() {
  tt::init_tables();
  CHECK_NEAR(tt::fsin(0.0f), 0.0, 0.01);
  CHECK_NEAR(tt::fsin(3.14159265f / 2.0f), 1.0, 0.01);
}

void register_tables_tests() {
  RUN_TEST(test_isin_matches_libm);
  RUN_TEST(test_fsin);
}
