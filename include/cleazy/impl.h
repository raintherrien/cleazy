#ifndef CLEAZY_IMPL_H_
#define CLEAZY_IMPL_H_

/*
 * CLEAZY_THREAD initializes thread local storage for profiling objects.
 * This macro must be called on a thread being profiled before any block
 * or function.
 */
#define CLEAZY_THREAD(NAME) cleazy_thread(NAME);

/*
 * CLEAZY_BK and CLEAZY_FN create a new block that is profiled and
 * terminated by CLEAZY_END.
 *
 * CLEAZY_FN is a helper macro that passes the current function name to
 * CLEAZY_BK.
 *
 * CLEAZY_BK and CLEAZY_FNC are identical to CLEAZY_BK and CLEAZY_FN
 * except that they also accept a uint32_t ARGB color value for the
 * easy_profiler GUI.
 */
#define CLEAZY_BKC(NAME,ARGB) do {                            \
        static const struct cleazy_dsc cleazy_dsc_local = {   \
            .name = NAME,                                     \
            .file = __FILE__,                                 \
            .line = __LINE__,                                 \
            .argb = ARGB                                      \
        };                                                    \
        struct cleazy_blk cleazy_blk_local = {                \
            .dsc = &cleazy_dsc_local, .begin = cleazy_nowns() \
        };
#define CLEAZY_FNC(ARGB) CLEAZY_BKC(__func__,ARGB)
#define CLEAZY_BK(NAME)  CLEAZY_BKC(NAME,0xffffffff)
#define CLEAZY_FN(NAME)  CLEAZY_FNC(0xffffffff)

/*
 * CLEAZY_END terminates and logs a block created by CLEAZY_BK or
 * CLEAZY_FN.
 *
 * If CLEAZY_PROFILE_SELF is defined then the insertion of that block is
 * also profiled. This is extremely bad for performance and shouldn't
 * normally be used, unless you suspect a bottleneck in this code.
 */
#ifdef CLEAZY_PROFILE_SELF
extern const struct cleazy_dsc cleazy_dsc_push;
# define CLEAZY_END() CLEAZY_END_PROFILE_SELF
#else
# define CLEAZY_END() CLEAZY_END_SIMPLE
#endif

#define CLEAZY_END_SIMPLE                                     \
        cleazy_blk_local.end = cleazy_nowns();                \
        cleazy_push(cleazy_blk_local);                        \
    } while (0)
#define CLEAZY_END_PROFILE_SELF                               \
        uint64_t cleazy_joinns = cleazy_nowns();              \
        struct cleazy_blk cleazy_blk_push = {                 \
            .dsc = &cleazy_dsc_push, .begin = cleazy_joinns   \
        };                                                    \
        cleazy_blk_local.end = cleazy_joinns;                 \
        cleazy_push(cleazy_blk_local);                        \
        cleazy_blk_push.end = cleazy_nowns();                 \
        cleazy_push(cleazy_blk_push);                         \
    } while (0)

/*
 * CLEAZY_PAUSE and CLEAZY_RESUME pause and resume profiling at runtime.
 */
#define CLEAZY_PAUSE()  (cleazy_pause())
#define CLEAZY_RESUME() (cleazy_resume())

/*
 * CLEAZY_FLUSH creates a new easy_profiler v2.1.0 file of all the
 * blocks created since the start of the application or the last flush.
 *
 * Onus is on the user of the library to ensure no threads are creating
 * new events while data is being written to disk. CLEAZY_PAUSE and
 * CLEAZY_RESUME are sufficient to prevent data races.
 *
 * Once complete, thread local data is left in a valid state with all
 * blocks written to disk and most allocated memory free.
 */
#define CLEAZY_FLUSH(FILENAME) (cleazy_flush(FILENAME))

/*
 * CLEAZY_CLEANUP frees allocated memory. This should be called once all
 * threads are complete profiling and data has been flushed to disk.
 * Unflushed blocks and all thread local data is freed.
 */
#define CLEAZY_CLEANUP() (cleazy_cleanup())

/*
 * These functions are the internal API of cleazy. Users should not call
 * them directly. Use the macros defined above instead.
 *
 * All these functions do their best to recover from error, exiting when
 * that's not possible. Not at all fit for production. :)
 *
 * easy_profiler supports a lot of runtime information that I've omitted
 * from cleazy_blk simply because I don't need it right now, and I would
 * rather have the tiny struct.
 *
 * cleazy_dsc block descriptors are declared const static at block scope
 * throughout the file by macros when profiling.
 *
 * cleazy_blk reference descriptors for most of their state, only
 * recording period information which forms the histogram.
 *
 * cleazy_cleanup frees thread superblocks and all block chunks.
 *
 * cleazy_flush coalesces thread local blocks and writes to a file in
 * the easy_profiler v2.1.0 file format. External synchronization is
 * required to ensure no threads are logging during this time. Threads
 * are left in an empty state. cleazy_pause and cleazy_resume are
 * sufficient to block threads from trampling data during a flush.
 *
 * cleazy_pause and cleazy_resume pause and resume profiling at runtime.
 *
 * cleazy_push pushes a block onto the thread local history.
 *
 * cleazy_thread sets up the current thread local superblock. This must
 * be called on each thread, before cleazy_blk.
 */

#include <cleazy/common.h>
#include <stdint.h>

struct cleazy_dsc {
    const char *name;
    const char *file;
    uint32_t    line;
    uint32_t    argb;
};

struct cleazy_blk {
    union {
        const struct cleazy_dsc *dsc;
        uint32_t dscid;
    };
    uint64_t begin;
    uint64_t end;
};

void cleazy_cleanup(void);
void cleazy_flush(const char *filename);
void cleazy_pause(void);
void cleazy_push(struct cleazy_blk);
void cleazy_resume(void);
void cleazy_thread(const char *thread_name);

#endif /* CLEAZY_IMPL_H_ */
