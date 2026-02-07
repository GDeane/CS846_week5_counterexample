#include "pool_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

static inline int is_power_of_two(size_t x) {
    return x && !(x & (x - 1));
}

int pool_internal_contains(MemoryPool* pool, void* ptr) {
    if (!pool || !pool->initialized || !ptr) return 0;
    uintptr_t base = (uintptr_t)pool->buf;
    uintptr_t p = (uintptr_t)ptr;
    if (p < base) return 0;
    uintptr_t offset = p - base;
    size_t total = pool->chunk_size * pool->block_count;
    if (offset >= total) return 0;
    if (offset % pool->chunk_size != 0) return 0;
    return 1;
}

void pool_free_unlocked(MemoryPool* pool, void* ptr) {
    if (!pool || !pool->initialized || !ptr) return;
    if (!pool_internal_contains(pool, ptr)) return;
    /* Insert at head of free list. The next pointer is stored in the first word of the block. */
    *((void**)ptr) = pool->free_list;
    pool->free_list = ptr;
}

int pool_init(MemoryPool* pool, size_t chunk_size, size_t block_count) {
    if (!pool || block_count == 0 || chunk_size < sizeof(void*) || !is_power_of_two(chunk_size)) return -1;

    void* buf = NULL;
    size_t total = chunk_size * block_count;
    int ret = posix_memalign(&buf, chunk_size, total);
    if (ret != 0 || buf == NULL) return -1;

    /* Initialize free list: each block stores a pointer to the next block in its first word. */
    for (size_t i = 0; i < block_count; ++i) {
        void* block = (char*)buf + i * chunk_size;
        void* next = (i + 1 < block_count) ? (char*)buf + (i + 1) * chunk_size : NULL;
        *((void**)block) = next;
    }

    pool->buf = buf;
    pool->free_list = buf;
    pool->chunk_size = chunk_size;
    pool->block_count = block_count;
    pool->initialized = 1;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(buf);
        pool->initialized = 0;
        return -1;
    }

    return 0;
}

void pool_destroy(MemoryPool* pool) {
    if (!pool || !pool->initialized) return;

    pthread_mutex_lock(&pool->lock);
    free(pool->buf);
    pool->buf = NULL;
    pool->free_list = NULL;
    pool->chunk_size = 0;
    pool->block_count = 0;
    pool->initialized = 0;
    pthread_mutex_unlock(&pool->lock);

    pthread_mutex_destroy(&pool->lock);
}

void* pool_alloc(MemoryPool* pool, size_t size, size_t align) {
    if (!pool || !pool->initialized) return NULL;
    if (size == 0 || size > pool->chunk_size) return NULL;
    if (!is_power_of_two(align) || align > pool->chunk_size) return NULL;

    pthread_mutex_lock(&pool->lock);
    if (pool->free_list == NULL) {
        pthread_mutex_unlock(&pool->lock);
        return NULL;
    }

    void* block = pool->free_list;
    pool->free_list = *((void**)block);
    pthread_mutex_unlock(&pool->lock);

    return block;
}

void pool_free(MemoryPool* pool, void* ptr) {
    if (!pool || !pool->initialized || ptr == NULL) return;

    pthread_mutex_lock(&pool->lock);
    if (!pool_internal_contains(pool, ptr)) {
        pthread_mutex_unlock(&pool->lock);
        return;
    }

    pool_free_unlocked(pool, ptr);
    pthread_mutex_unlock(&pool->lock);
}
