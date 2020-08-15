#include "cleazy/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static const struct timespec to = { .tv_sec = 0, .tv_nsec = 10000 };

void
foo(void)
{
    CLEAZY_FNC(0xffff0000);
    (void) nanosleep(&to, NULL);
    CLEAZY_END();
}

void
bar(void)
{
    CLEAZY_FNC(0xff00ff00);
    (void) nanosleep(&to, NULL);
    CLEAZY_END();
}

void
boo(int width)
{
    CLEAZY_FNC(0xff0000ff);
    for (int i = 0; i < width; ++ i) {
        if (i % 2 == 0)
            foo();
        else
            bar();
    }
    if (width > 0) {
        boo(width - 1);
    }
    CLEAZY_END();
}

int
main(void)
{
    CLEAZY_THREAD("Main");
    boo(8);
    CLEAZY_FLUSH("simple.prof");
    CLEAZY_CLEANUP();
    return EXIT_SUCCESS;
}
