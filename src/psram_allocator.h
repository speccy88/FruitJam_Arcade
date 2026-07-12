#ifndef PSRAM_ALLOCATOR_H
#define PSRAM_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Fruit Jam external PSRAM and its TLSF heap.
// The call is idempotent. It returns false when PSRAM is not detected or the
// allocator cannot be created.
bool arcade_psram_init(void);

bool arcade_psram_ready(void);
size_t arcade_psram_capacity(void);
void *arcade_psram_alloc(size_t bytes);
void *arcade_psram_alloc_aligned(size_t alignment, size_t bytes);
void *arcade_psram_realloc(void *ptr, size_t bytes);
void arcade_psram_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
