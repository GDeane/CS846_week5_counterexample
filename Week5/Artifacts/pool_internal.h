#ifndef POOL_INTERNAL_H
#define POOL_INTERNAL_H

/* Internal-only header. Do NOT expose to public API consumers. */

#include <stddef.h>
#include <pthread.h>
#include "pool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MemoryPool {
    void* buf;           /* start of allocated buffer */
    void* free_list;     /* head of free list (blocks store next pointer in first word) */
    size_t chunk_size;   /* size of each block */
    size_t block_count;  /* number of blocks in buffer */
    pthread_mutex_t lock; /* internal lock — callers of internal APIs must hold this */
    int initialized;     /* non-zero when pool_init has been called successfully */
};

/**
 * @brief Returns the block to the pool's internal free list.
 *
 * Inserts ptr into the pool's internal free list for reuse; does not perform boundary checks.
 *
 * @param pool Memory pool to return to (must not be NULL)
 * @param ptr Pointer previously obtained from pool_alloc
 * @return None
 *
 * @pre Caller must hold pool->lock and ptr must be aligned to pool->chunk_size.
 * @exceptsafe Basic (may allocate for debug logging)
 */
void pool_free(MemoryPool* pool, void* ptr);

/**
 * @brief Returns whether ptr belongs to this pool and is properly aligned.
 *
 * @return 1 if pointer is valid, 0 otherwise
 */
int pool_internal_contains(MemoryPool* pool, void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* POOL_INTERNAL_H */
