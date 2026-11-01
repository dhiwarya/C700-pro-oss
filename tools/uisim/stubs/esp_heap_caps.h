#pragma once
/* Desktop stand-in (tools/uisim): plain malloc. */
#include <stdlib.h>
#include <stddef.h>
#define MALLOC_CAP_SPIRAM   (1 << 10)
#define MALLOC_CAP_INTERNAL (1 << 11)
#define MALLOC_CAP_8BIT     (1 << 2)
#define MALLOC_CAP_DMA      (1 << 3)
#define heap_caps_malloc(n, caps) malloc(n)
#define heap_caps_calloc(c, n, caps) calloc(c, n)
#define heap_caps_free(p) free(p)
static inline size_t heap_caps_get_free_size(unsigned caps) { return caps & MALLOC_CAP_SPIRAM ? 7600u * 1024 : 158u * 1024; }
static inline size_t heap_caps_get_largest_free_block(unsigned caps) { return heap_caps_get_free_size(caps) / 2; }
