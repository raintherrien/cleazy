#ifndef CLEAZY_PROFILER_H_
#define CLEAZY_PROFILER_H_

/* Profiling isn't performed unless CLEAZY_PROFILE is defined. */
#ifdef CLEAZY_PROFILE
# include <cleazy/impl.h>
#else
# include <cleazy/stub.h>
#endif

#endif /* CLEAZY_PROFILER_H_ */
