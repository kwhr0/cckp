#define va_list			long long *
#define va_start(p, s)	((p) = (long long *)&(s) + 1)
#define va_arg(p, t)	(*(t *)(p)++)
#define va_end(p)
