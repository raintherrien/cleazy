#ifndef CLEAZY_COMMON_H_
#define CLEAZY_COMMON_H_

#include <stdint.h>
#include <time.h>

/*
 * Simple means of reading current nanosecond since epoch.
 * TODO: Because of this little function we require POSIX.1b
 */
static inline uint64_t
cleazy_nowns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

#endif /* CLEAZY_COMMON_H_ */
