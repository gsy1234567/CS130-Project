#include "fix_point.h"


#define f (1 << 14)

fp1714_t to_fix_point(int32_t n) { return n * f; }

int round_to_zero(fp1714_t x) { return x / f; }

int round_to_nearest(fp1714_t x) { return x >= 0 ? (x + f / 2) / f : (x - f / 2) / f; }

fp1714_t fp2_add(fp1714_t x, fp1714_t y) { return x + y; }
fp1714_t fp2_sub(fp1714_t x, fp1714_t y) { return x - y; }
fp1714_t fp_int_add(fp1714_t x, int n) { return x + n * f; }
fp1714_t fp_int_sub(fp1714_t x, int n) { return x - n * f; }
fp1714_t fp2_mul(fp1714_t x, fp1714_t y) { return ((int64_t)x) * y / f; }
fp1714_t fp_int_mul(fp1714_t x, int n) { return x * n; }
fp1714_t fp2_div(fp1714_t x, fp1714_t y) { return ((int64_t)x) * f / y; }

fp1714_t fp_int_div(fp1714_t x, int n) { return x / n; }

#undef f