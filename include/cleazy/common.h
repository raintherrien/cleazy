#ifndef CLEAZY_COMMON_H_
#define CLEAZY_COMMON_H_

#include <stdint.h>

/*
 * Returns current nanosecond. Starting point is arbitrary relative to
 * the start of profling.
 */
uint64_t cleazy_nowns(void);

#endif /* CLEAZY_COMMON_H_ */
