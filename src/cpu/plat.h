/*
 * wbox - Platform interface stub
 * Stub header replacing 86box/plat.h
 */
#ifndef WBOX_PLAT_H
#define WBOX_PLAT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#    define UNUSED(arg) arg
#else
#    define UNUSED(arg) __attribute__((unused)) arg
#endif

#ifndef fallthrough
#  ifdef _MSC_VER
#    define fallthrough do {} while (0)
#  else
#    if __has_attribute(fallthrough)
#      define fallthrough __attribute__((fallthrough))
#    else
#      define fallthrough do {} while (0)
#    endif
#  endif
#endif

/* Memory allocation */
static inline void *plat_falloc(size_t size) { return malloc(size); }
static inline void  plat_ffree(void *ptr) { free(ptr); }

/* Executable memory allocation for dynarec */
extern void *plat_mmap(size_t size, uint8_t executable);
extern void  plat_munmap(void *ptr, size_t size);

#endif /* WBOX_PLAT_H */
