/* gcc 2.95 ginclude/va-ppc.h (System V.4): va_list for ProDG-compiled game and libc code. */
#ifndef VA_PPC_H
#define VA_PPC_H

typedef struct __va_list_tag {
    char gpr;   /* SN: signed (gcc's ginclude has unsigned char) */
    char fpr;
    char *overflow_arg_area;
    char *reg_save_area;
} __va_list[1], __gnuc_va_list[1];
typedef __gnuc_va_list va_list;

#ifdef __cplusplus
/* g++ 2.95 does not inline __builtin_memcpy; the struct copy gives the original 3-word copy */
#define va_start(AP, LASTARG) \
    (__builtin_next_arg(LASTARG), *(AP) = *(struct __va_list_tag*) __builtin_saveregs())
#else
#define va_start(AP, LASTARG) \
    (__builtin_next_arg(LASTARG), __builtin_memcpy((AP), __builtin_saveregs(), sizeof(__gnuc_va_list)))
#endif
#define va_end(AP) ((void)0)

/* SN's va-ppc.h (v393/include, "varargs fixes" 2001) va_arg: gpr/fpr are signed `char', the integer
 * case tests `gpr + size <= 8'. Needed by the newlib vf* units. */
#ifndef VA_PPC_ARG
#define VA_PPC_ARG
#define __va_overflow(AP) (AP)->overflow_arg_area
typedef struct {
    long __gp_save[8];   /* save area for GP registers */
    double __fp_save[8]; /* save area for FP registers */
} __va_regsave_t;
#define __VA_FP_REGSAVE(AP, TYPE) ((TYPE*) (void*) (&(((__va_regsave_t*) (AP)->reg_save_area)->__fp_save[(int) (AP)->fpr])))
#define __VA_GP_REGSAVE(AP, TYPE) ((TYPE*) (void*) (&(((__va_regsave_t*) (AP)->reg_save_area)->__gp_save[(int) (AP)->gpr])))
#define __va_float_p(TYPE) (__builtin_classify_type(*(TYPE*) 0) == 8)
#define __va_aggregate_p(TYPE) (__builtin_classify_type(*(TYPE*) 0) >= 12)
#define __va_size(TYPE) ((sizeof(TYPE) + sizeof(long) - 1) / sizeof(long))
#define __va_longlong_p(TYPE) ((__builtin_classify_type(*(TYPE*) 0) == 1) && (sizeof(TYPE) == 8))

#define va_arg(AP,TYPE)							\
__extension__ (*({							\
  register TYPE *__ptr;							\
									\
  if (__va_float_p (TYPE) && (AP)->fpr < 8)				\
    {									\
      __ptr = __VA_FP_REGSAVE (AP, TYPE);				\
      (AP)->fpr++;							\
    }									\
									\
  else if (__va_aggregate_p (TYPE) && (AP)->gpr < 8)			\
    {									\
      __ptr = * __VA_GP_REGSAVE (AP, TYPE *);				\
      (AP)->gpr++;							\
    }									\
									\
  else if (!__va_float_p (TYPE) && !__va_aggregate_p (TYPE)		\
	   && (AP)->gpr + __va_size(TYPE) <= 8				\
	   && (!__va_longlong_p(TYPE)					\
	       || (AP)->gpr + __va_size(TYPE) <= 8))			\
    {									\
      if (__va_longlong_p(TYPE) && ((AP)->gpr & 1) != 0)		\
	(AP)->gpr++;							\
									\
      __ptr = __VA_GP_REGSAVE (AP, TYPE);				\
      (AP)->gpr += __va_size (TYPE);					\
    }									\
									\
  else if (!__va_float_p (TYPE) && !__va_aggregate_p (TYPE)		\
	   && (AP)->gpr < 8)						\
    {									\
      (AP)->gpr = 8;							\
      __ptr = (TYPE *) (void *) (__va_overflow(AP));			\
      __va_overflow(AP) += __va_size (TYPE) * sizeof (long);		\
    }									\
									\
  else if (__va_aggregate_p (TYPE))					\
    {									\
      __ptr = * (TYPE **) (void *) (__va_overflow(AP));			\
      __va_overflow(AP) += sizeof (TYPE *);				\
    }									\
  else									\
    {									\
      __ptr = (TYPE *) (void *) (__va_overflow(AP));			\
      __va_overflow(AP) += __va_size (TYPE) * sizeof (long);		\
    }									\
									\
  __ptr;								\
}))
#endif /* VA_PPC_ARG */

#endif
