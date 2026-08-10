/* newlib 1.8.2 libm/common/fdlibm.h, as used by the SN ProDG libm: the __ieee754_* entry points
 * are the public names (no w_*.c wrappers were linked). */
#ifndef FDLIBM_H
#define FDLIBM_H

#define __IEEE_BIG_ENDIAN
typedef unsigned int __uint32_t;
typedef int __int32_t;

#define __P(p) p

#define HUGE ((float)3.40282346638528860e+38)
#define X_TLOSS 1.41484755040568800000e+16

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ieee754_sqrt
#define __ieee754_sqrt sqrt
#endif
#ifndef __ieee754_pow
#define __ieee754_pow pow
#endif
#ifndef __ieee754_log
#define __ieee754_log log
#endif
#ifndef __ieee754_log10
#define __ieee754_log10 log10
#endif
#ifndef __ieee754_atan2
#define __ieee754_atan2 atan2
#endif
#ifndef __ieee754_fmod
#define __ieee754_fmod fmod
#endif
#define __ieee754_sqrtf sqrtf
#define __ieee754_powf powf
#define __ieee754_acosf acosf
#define __ieee754_asinf asinf
#define __ieee754_atan2f atan2f

/* ieee style elementary functions */
extern double __ieee754_sqrt __P((double));
extern double __ieee754_pow __P((double, double));
extern double __ieee754_log __P((double));
extern double __ieee754_log10 __P((double));
extern double __ieee754_atan2 __P((double, double));
extern double __ieee754_fmod __P((double, double));
extern int __ieee754_rem_pio2 __P((double, double *));

extern double atan __P((double));
extern double fabs __P((double));
extern double floor __P((double));
extern double scalbn __P((double, int));
extern double copysign __P((double, double));
extern double cos __P((double));
extern double tan __P((double));

/* fdlibm kernel function */
extern double __kernel_sin __P((double, double, int));
extern double __kernel_cos __P((double, double));
extern double __kernel_tan __P((double, double, int));
extern int __kernel_rem_pio2 __P((double *, double *, int, int, int, const __int32_t *));

/* ieee style elementary float functions */
extern float __ieee754_sqrtf __P((float));
extern float __ieee754_powf __P((float, float));
extern float __ieee754_acosf __P((float));
extern float __ieee754_asinf __P((float));
extern float __ieee754_atan2f __P((float, float));
extern __int32_t __ieee754_rem_pio2f __P((float, float *));

extern float atanf __P((float));
extern float fabsf __P((float));
extern float floorf __P((float));
extern float scalbnf __P((float, int));
extern float copysignf __P((float, float));
extern float cosf __P((float));
extern float sinf __P((float));
extern float tanf __P((float));

/* float versions of fdlibm kernel functions */
extern float __kernel_sinf __P((float, float, int));
extern float __kernel_cosf __P((float, float));
extern float __kernel_tanf __P((float, float, int));
extern int __kernel_rem_pio2f __P((float *, float *, int, int, int, const __int32_t *));

#ifdef __cplusplus
}
#endif

/* A union which permits us to convert between a double and two 32 bit ints.  */
typedef union {
    double value;
    struct {
        __uint32_t msw;
        __uint32_t lsw;
    } parts;
} ieee_double_shape_type;

/* Get two 32 bit ints from a double.  */
#define EXTRACT_WORDS(ix0, ix1, d) \
    do { \
        ieee_double_shape_type ew_u; \
        ew_u.value = (d); \
        (ix0) = ew_u.parts.msw; \
        (ix1) = ew_u.parts.lsw; \
    } while (0)

/* Get the more significant 32 bit int from a double.  */
#define GET_HIGH_WORD(i, d) \
    do { \
        ieee_double_shape_type gh_u; \
        gh_u.value = (d); \
        (i) = gh_u.parts.msw; \
    } while (0)

/* Get the less significant 32 bit int from a double.  */
#define GET_LOW_WORD(i, d) \
    do { \
        ieee_double_shape_type gl_u; \
        gl_u.value = (d); \
        (i) = gl_u.parts.lsw; \
    } while (0)

/* Set a double from two 32 bit ints.  */
#define INSERT_WORDS(d, ix0, ix1) \
    do { \
        ieee_double_shape_type iw_u; \
        iw_u.parts.msw = (ix0); \
        iw_u.parts.lsw = (ix1); \
        (d) = iw_u.value; \
    } while (0)

/* Set the more significant 32 bits of a double from an int.  */
#define SET_HIGH_WORD(d, v) \
    do { \
        ieee_double_shape_type sh_u; \
        sh_u.value = (d); \
        sh_u.parts.msw = (v); \
        (d) = sh_u.value; \
    } while (0)

/* Set the less significant 32 bits of a double from an int.  */
#define SET_LOW_WORD(d, v) \
    do { \
        ieee_double_shape_type sl_u; \
        sl_u.value = (d); \
        sl_u.parts.lsw = (v); \
        (d) = sl_u.value; \
    } while (0)

/* A union which permits us to convert between a float and a 32 bit int.  */
typedef union {
    float value;
    __uint32_t word;
} ieee_float_shape_type;

/* Get a 32 bit int from a float.  */
#define GET_FLOAT_WORD(i, d) \
    do { \
        ieee_float_shape_type gf_u; \
        gf_u.value = (d); \
        (i) = gf_u.word; \
    } while (0)

/* Set a float from a 32 bit int.  */
#define SET_FLOAT_WORD(d, i) \
    do { \
        ieee_float_shape_type sf_u; \
        sf_u.word = (i); \
        (d) = sf_u.value; \
    } while (0)

#endif
