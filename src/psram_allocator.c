#include "psram_allocator.h"

#include "psram.h"
#include "tlsf/tlsf.h"

static tlsf_t psram_heap;
static size_t psram_capacity;

bool arcade_psram_init(void) {
    if (psram_heap) return true;

    psram_capacity = setup_psram();
    if (psram_capacity == 0) return false;

    psram_heap = tlsf_create_with_pool((void *)PSRAM_BASE, psram_capacity);
    if (!psram_heap) {
        psram_capacity = 0;
        return false;
    }
    return true;
}

bool arcade_psram_ready(void) {
    return psram_heap != NULL;
}

size_t arcade_psram_capacity(void) {
    return psram_capacity;
}

void *arcade_psram_alloc(size_t bytes) {
    return psram_heap ? tlsf_malloc(psram_heap, bytes) : NULL;
}

void *arcade_psram_alloc_aligned(size_t alignment, size_t bytes) {
    return psram_heap ? tlsf_memalign(psram_heap, alignment, bytes) : NULL;
}

void *arcade_psram_realloc(void *ptr, size_t bytes) {
    return psram_heap ? tlsf_realloc(psram_heap, ptr, bytes) : NULL;
}

void arcade_psram_free(void *ptr) {
    if (psram_heap && ptr) tlsf_free(psram_heap, ptr);
}
