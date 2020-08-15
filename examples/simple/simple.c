#include "cleazy/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
foo(void)
{
    CLEAZY_FNC(0xffff0000);
    (void) usleep(10);
    CLEAZY_END();
}

void
bar(void)
{
    CLEAZY_FNC(0xff00ff00);
    (void) usleep(10);
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
