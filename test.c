/**
 * IPC protocol tests: unique (timestamp, sequence) across processes and
 * threads, overlapping open/close, reopen after last close.
 */
#define UUIDV7_S_NAME "uuidv7_test"
#define UUIDV7_IMPLEMENTATION
#include "uuidv7.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL: %s\n", msg);                                \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static void ipc_cleanup(void) {
    sem_unlink("/" UUIDV7_S_NAME "_sem");
    shm_unlink("/" UUIDV7_S_NAME "_shm");
}

static int uuid_cmp(const void *a, const void *b) {
    const uuidv7_t *ua = a, *ub = b;
    if (ua->uuid_high < ub->uuid_high) { return -1; }
    if (ua->uuid_high > ub->uuid_high) { return 1; }
    if (ua->uuid_low < ub->uuid_low)   { return -1; }
    if (ua->uuid_low > ub->uuid_low)   { return 1; }
    return 0;
}

static int tsseq_cmp(const void *a, const void *b) {
    const uuidv7_t *ua = a, *ub = b;
    uint64_t tsa = ua->uuid_high >> (UUIDV7_VERSION_BS + UUIDV7_SEQ_BS);
    uint64_t tsb = ub->uuid_high >> (UUIDV7_VERSION_BS + UUIDV7_SEQ_BS);
    if (tsa < tsb) { return -1; }
    if (tsa > tsb) { return 1; }
    uint64_t sa = ua->uuid_high & UUIDV7_SEQ_MASK;
    uint64_t sb = ub->uuid_high & UUIDV7_SEQ_MASK;
    if (sa < sb) { return -1; }
    if (sa > sb) { return 1; }
    return 0;
}

static int count_dups(uuidv7_t *ids, size_t n, int (*cmp)(const void *, const void *)) {
    qsort(ids, n, sizeof(*ids), cmp);
    int dups = 0;
    for (size_t i = 1; i < n; i++) {
        if (cmp(&ids[i - 1], &ids[i]) == 0) { dups++; }
    }
    return dups;
}

static void test_reopen(void) {
    uuidv7_ctx_t ctx;
    CHECK(uuidv7_open(&ctx, 1), "reopen: first open");
    uuidv7_t a = uuidv7_get(&ctx);
    CHECK(uuidv7_is_valid(a), "reopen: first get");
    uuidv7_close(&ctx);

    CHECK(uuidv7_open(&ctx, 1), "reopen: second open");
    uuidv7_t b = uuidv7_get(&ctx);
    CHECK(uuidv7_is_valid(b), "reopen: second get");
    uuidv7_close(&ctx);
}

static void test_monotonic(void) {
    uuidv7_ctx_t ctx;
    CHECK(uuidv7_open(&ctx, 1), "monotonic: open");
    uuidv7_t prev = uuidv7_get(&ctx);
    CHECK(uuidv7_is_valid(prev), "monotonic: first");
    for (int i = 0; i < 10000; i++) {
        uuidv7_t id = uuidv7_get(&ctx);
        if (!uuidv7_is_valid(id) || id.uuid_high < prev.uuid_high) {
            CHECK(0, "monotonic: uuid_high went backwards or invalid");
            break;
        }
        prev = id;
    }
    uuidv7_close(&ctx);
}

static void test_multiprocess(void) {
    const int nproc = 8;
    const int nids = 3000;
    const size_t n = (size_t)nproc * (size_t)nids;
    uuidv7_t *ids = mmap(NULL, n * sizeof(*ids), PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    CHECK(ids != MAP_FAILED, "multiprocess: mmap");
    if (ids == MAP_FAILED) { return; }

    pid_t pids[8];
    int spawned = 0;
    for (int p = 0; p < nproc; p++) {
        pid_t pid = fork();
        if (pid == 0) {
            uuidv7_ctx_t ctx;
            if (!uuidv7_open(&ctx, (uint16_t)p)) { _exit(2); }
            for (int i = 0; i < nids; i++) {
                uuidv7_t id = uuidv7_get(&ctx);
                if (!uuidv7_is_valid(id)) { uuidv7_close(&ctx); _exit(3); }
                ids[(size_t)p * (size_t)nids + (size_t)i] = id;
            }
            uuidv7_close(&ctx);
            _exit(0);
        }
        if (pid < 0) {
            CHECK(0, "multiprocess: fork");
            break;
        }
        pids[spawned++] = pid;
    }

    int bad = 0;
    for (int p = 0; p < spawned; p++) {
        int st = 0;
        waitpid(pids[p], &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { bad++; }
    }
    CHECK(bad == 0, "multiprocess: child exit");
    CHECK(spawned == nproc, "multiprocess: spawned all");
    if (spawned == nproc && bad == 0) {
        CHECK(count_dups(ids, n, uuid_cmp) == 0, "multiprocess: unique UUIDs");
        CHECK(count_dups(ids, n, tsseq_cmp) == 0,
              "multiprocess: unique (timestamp, sequence)");
    }
    munmap(ids, n * sizeof(*ids));
}

struct targs {
    uuidv7_ctx_t *ctx;
    uuidv7_t *out;
    int n;
    int fail;
};

static void *thread_fn(void *arg) {
    struct targs *a = arg;
    for (int i = 0; i < a->n; i++) {
        uuidv7_t id = uuidv7_get(a->ctx);
        if (!uuidv7_is_valid(id)) {
            a->fail = 1;
            break;
        }
        a->out[i] = id;
    }
    return NULL;
}

static void test_multithread(void) {
    const int nthr = 8;
    const int nids = 3000;
    const size_t n = (size_t)nthr * (size_t)nids;
    uuidv7_t *ids = calloc(n, sizeof(*ids));
    CHECK(ids != NULL, "multithread: alloc");
    if (!ids) { return; }

    uuidv7_ctx_t ctx;
    CHECK(uuidv7_open(&ctx, 7), "multithread: open");

    pthread_t th[8];
    struct targs args[8];
    int started = 0;
    for (int t = 0; t < nthr; t++) {
        args[t].ctx = &ctx;
        args[t].out = ids + (size_t)t * (size_t)nids;
        args[t].n = nids;
        args[t].fail = 0;
        if (pthread_create(&th[t], NULL, thread_fn, &args[t]) != 0) {
            CHECK(0, "multithread: pthread_create");
            break;
        }
        started++;
    }
    for (int t = 0; t < started; t++) {
        pthread_join(th[t], NULL);
        CHECK(args[t].fail == 0, "multithread: get failed");
    }
    uuidv7_close(&ctx);

    if (started == nthr) {
        CHECK(count_dups(ids, n, uuid_cmp) == 0, "multithread: unique UUIDs");
        CHECK(count_dups(ids, n, tsseq_cmp) == 0,
              "multithread: unique (timestamp, sequence)");
    }
    free(ids);
}

static void test_overlap_close(void) {
    _Atomic(int) *stage = mmap(NULL, sizeof(*stage), PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    CHECK(stage != MAP_FAILED, "overlap: mmap");
    if (stage == MAP_FAILED) { return; }
    atomic_store(stage, 0);

    uuidv7_ctx_t parent;
    CHECK(uuidv7_open(&parent, 1), "overlap: parent open");

    pid_t pid = fork();
    if (pid == 0) {
        uuidv7_ctx_t ctx;
        if (!uuidv7_open(&ctx, 2)) { _exit(2); }
        atomic_store(stage, 1);
        while (atomic_load(stage) != 2) { }

        for (int i = 0; i < 500; i++) {
            uuidv7_t id = uuidv7_get(&ctx);
            if (!uuidv7_is_valid(id)) { uuidv7_close(&ctx); _exit(3); }
        }
        uuidv7_close(&ctx);
        _exit(0);
    }
    if (pid < 0) {
        CHECK(0, "overlap: fork");
        uuidv7_close(&parent);
        munmap(stage, sizeof(*stage));
        return;
    }

    while (atomic_load(stage) != 1) { }
    uuidv7_close(&parent);
    atomic_store(stage, 2);

    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0,
          "overlap: child still generated after parent close");

    CHECK(uuidv7_open(&parent, 1), "overlap: parent reopen");
    uuidv7_t id = uuidv7_get(&parent);
    CHECK(uuidv7_is_valid(id), "overlap: get after reopen");
    uuidv7_close(&parent);
    munmap(stage, sizeof(*stage));
}

int main(void) {
    ipc_cleanup();

    test_reopen();
    test_monotonic();
    test_multiprocess();
    test_multithread();
    test_overlap_close();

    ipc_cleanup();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("ipc protocol tests ok\n");
    return EXIT_SUCCESS;
}
