#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "pool.h"
#include "pool_internal.h"

static int run_test(const char* name, int (*fn)(void)) {
    int rc = fn();
    if (rc == 0) {
        printf("[PASS] %s\n", name);
    } else {
        printf("[FAIL] %s\n", name);
    }
    return rc;
}

static int test_basic_alloc_free(void) {
    MemoryPool pool;
    if (pool_init(&pool, 32, 4) != 0) return 1;

    void* p = pool_alloc(&pool, 16, 8);
    if (!p) { pool_destroy(&pool); return 1; }
    memset(p, 0xAA, 16);
    pool_free(&pool, p);

    pool_destroy(&pool);
    return 0;
}

static int test_exhaustion(void) {
    MemoryPool pool;
    if (pool_init(&pool, 32, 2) != 0) return 1;

    void* a = pool_alloc(&pool, 8, 8);
    void* b = pool_alloc(&pool, 8, 8);
    void* c = pool_alloc(&pool, 8, 8);

    if (!a || !b) { pool_destroy(&pool); return 1; }
    if (c != NULL) { pool_destroy(&pool); return 1; }

    pool_free(&pool, a);
    void* d = pool_alloc(&pool, 8, 8);
    if (!d) { pool_destroy(&pool); return 1; }

    pool_free(&pool, b);
    pool_free(&pool, d);
    pool_destroy(&pool);
    return 0;
}

static int test_alignment_and_size(void) {
    MemoryPool pool;
    if (pool_init(&pool, 32, 4) != 0) return 1;

    void* p = pool_alloc(&pool, 64, 8);
    if (p != NULL) { pool_destroy(&pool); return 1; }

    p = pool_alloc(&pool, 16, 64);
    if (p != NULL) { pool_destroy(&pool); return 1; }

    p = pool_alloc(&pool, 16, 3);
    if (p != NULL) { pool_destroy(&pool); return 1; }

    pool_destroy(&pool);
    return 0;
}

static int test_null_and_invalid_free(void) {
    MemoryPool pool;
    if (pool_init(&pool, 32, 4) != 0) return 1;

    pool_free(&pool, NULL); // should be a no-op

    int x = 0;
    pool_free(&pool, &x); // invalid pointer; should be ignored

    pool_destroy(&pool);
    return 0;
}

static int test_internal_unlocked_free(void) {
    MemoryPool pool;
    if (pool_init(&pool, 32, 4) != 0) return 1;

    void* p = pool_alloc(&pool, 16, 8);
    if (!p) { pool_destroy(&pool); return 1; }

    /* Acquire lock and call internal, unlocked free */
    if (pthread_mutex_lock(&pool.lock) != 0) { pool_destroy(&pool); return 1; }
    pool_free_unlocked(&pool, p);
    /* Now the pool should hand back the block */
    void* q = pool_alloc(&pool, 16, 8);
    if (!q) { pthread_mutex_unlock(&pool.lock); pool_destroy(&pool); return 1; }
    pthread_mutex_unlock(&pool.lock);

    pool_free(&pool, q);
    pool_destroy(&pool);
    return 0;
}

int main(void) {
    int failures = 0;
    failures += run_test("basic_alloc_free", test_basic_alloc_free);
    failures += run_test("exhaustion", test_exhaustion);
    failures += run_test("alignment_and_size", test_alignment_and_size);
    failures += run_test("null_and_invalid_free", test_null_and_invalid_free);
    failures += run_test("internal_unlocked_free", test_internal_unlocked_free);

    if (failures == 0) {
        printf("All tests passed\n");
        return 0;
    }
    printf("%d test(s) failed\n", failures);
    return 1;
}
