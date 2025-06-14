/* --- project1.3 --- */
#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/*
The following table summarizes how fixed-point arithmetic operations 
can be implemented in C. In the table, x and y are fixed-point numbers,
n is an integer, fixed-point numbers are in signed p.q format where
p + q = 31, and f is 1 << q:

Convert n to fixed point:	n * f
Convert x to integer (rounding toward zero):	x / f
Convert x to integer (rounding to nearest):	(x + f / 2) / f if x >= 0,
(x - f / 2) / f if x <= 0.
Add x and y:	x + y
Subtract y from x:	x - y
Add x and n:	x + n * f
Subtract n from x:	x - n * f
Multiply x by y:	((int64_t) x) * y / f
Multiply x by n:	x * n
Divide x by y:	((int64_t) x) * f / y
Divide x by n:	x / n
*/

/* Type */
typedef int fpr; /* fixed-point real number */

/* Constants */
#define FP_P 17
#define FP_Q 14
#define FP_F (1<<FP_Q)

/* Functions */
#define FP_CONV_FP(n) ((n) * FP_F)
#define FP_CONV_INT_ZERO(x) ((x) / FP_F)
#define FP_CONV_INT_NEAR(x) (((x) >= 0) ? (((x) + FP_F / 2) / FP_F) : (((x) - FP_F / 2) / FP_F))
#define FP_ADD_FP(x, y) ((x) + (y))
#define FP_SUB_FP(x, y) ((x) - (y))
#define FP_ADD_INT(x, n) ((x) + FP_CONV_FP (n))
#define FP_SUB_INT(x, n) ((x) - FP_CONV_FP (n))
#define FP_MUL_FP(x, y) (((int64_t) (x)) * (y) / FP_F)
#define FP_MUL_INT(x, n) ((x) * (n))
#define FP_DIV_FP(x, y) (((int64_t) (x)) * FP_F / (y))
#define FP_DIV_INT(x, n) ((x) / (n))

#endif /* threads/fixed-point.h */
/* === project1.3 === */