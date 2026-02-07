#ifndef POOL_H
#define POOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MemoryPool MemoryPool;

//TODO: Document this
int pool_init(MemoryPool* pool, size_t chunk_size, size_t block_count);

//TODO: Document this
void pool_destroy(MemoryPool* pool);

//TODO: Document this
void* pool_alloc(MemoryPool* pool, size_t size, size_t align);

//TODO: Document this
void pool_free(MemoryPool* pool, void* ptr);

#ifdef __cplusplus
}
#endif

#endif /* POOL_H */
