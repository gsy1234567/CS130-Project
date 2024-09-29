#pragma once

#include <stdint.h>

#include <stdint.h>

typedef int32_t fp1714_t;
fp1714_t to_fix_point(int32_t n);
int round_to_zero(fp1714_t x);
int round_to_nearest(fp1714_t x);
fp1714_t fp2_add(fp1714_t x, fp1714_t y);
fp1714_t fp2_sub(fp1714_t x, fp1714_t y);
fp1714_t fp_int_add(fp1714_t x, int n);
fp1714_t fp_int_sub(fp1714_t x, int n);
fp1714_t fp2_mul(fp1714_t x, fp1714_t y);
fp1714_t fp_int_mul(fp1714_t x, int n);
fp1714_t fp2_div(fp1714_t x, fp1714_t y);
fp1714_t fp_int_div(fp1714_t x, int n);