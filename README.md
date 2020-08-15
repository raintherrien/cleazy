# cleazy

### Pronounced with a soft 'C'

A sketchy C profiler that traces to the [easy_profiler](https://github.com/yse/easy_profiler/) 2.1.0 file format.

[easy_profiler](https://github.com/yse/easy_profiler/) is a wonderful library and GUI, much better than this one, but has no C bindings!

# Supported features

Pretty much squat. More features will be added if/when I need them. Currently the only event types are function and compile time blocks. No arbitrary values, runtime named blocks, or context switch support.

Look to `include/cleazy/impl.h` for an explanation of the interface.

If this sounds like a bit of a hack job to you, that's because it is! Enjoy!
