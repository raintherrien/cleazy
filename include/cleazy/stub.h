#ifndef CLEAZY_STUB_H_
#define CLEAZY_STUB_H_

/*
 * Stub versions of macro API, for when profiling is disabled at compile
 * time. See impl.h for profiling implementations.
 */

#define CLEAZY_THREAD(...)
#define CLEAZY_BK(...)
#define CLEAZY_BKC(...)
#define CLEAZY_FN(...)
#define CLEAZY_FNC(...)
#define CLEAZY_END()
#define CLEAZY_PAUSE()
#define CLEAZY_RESUME()
#define CLEAZY_FLUSH(...)
#define CLEAZY_CLEANUP()

#endif /* CLEAZY_STUB_H_ */
