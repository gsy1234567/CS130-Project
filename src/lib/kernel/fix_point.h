#ifndef __LIB_KERNEL_FIX_POINT_H
#define __LIB_KERNEL_FIX_POINT_H

#include <stdint.h>
#include <stdbool.h>

typedef int64_t fixed_t;

#define FP_DECIMAL_DIGITS 14

#define FP_CONSTANT(n) (((fixed_t)n) << FP_DECIMAL_DIGITS)
#define FP_ADD(lhs, rhs) (((fixed_t)lhs) + rhs)
#define FP_SUB(lhs, rhs) (((fixed_t)lhs) - rhs)
#define FP_MUL(lhs, rhs) (((fixed_t)lhs) * rhs >> FP_DECIMAL_DIGITS)
#define FP_DIV(lhs, rhs) ((((fixed_t)lhs) << FP_DECIMAL_DIGITS) / rhs)
#define FP_ADD_INT(FP, INT) FP_ADD(FP, FP_CONSTANT(INT))
#define FP_SUB_INT(FP, INT) FP_SUB(FP, FP_CONSTANT(INT))
#define INT_SUB_FP(INT, FP) FP_SUB(FP_CONSTANT(INT), FP)
#define FP_MUL_INT(FP, INT) ((fixed_t)FP * INT)
#define FP_DIV_INT(FP, INT) ((fixed_t)FP / INT)
#define INT_DIV_FP(INT, FP) FP_DIV(FP_CONSTANT(INT), FP)
#define FP_ROUND_NEAREST(FP) ((FP < 0) ? (FP - (1 << FP_DECIMAL_DIGITS - 1)) / (1 << FP_DECIMAL_DIGITS) :  (FP + (1 << FP_DECIMAL_DIGITS - 1)) / (1 << FP_DECIMAL_DIGITS))


#endif 