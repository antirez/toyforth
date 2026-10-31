This is a work in progress toy interpreter for a FORTH-alike
programming language, developed during my YouTube C coding course [here](https://www.youtube.com/watch?v=HjXBXBgfKyk&list=PLrEMgOSrS_3cFJpM2gdw8EGFyRBZOyAKY)

The interpreter will be finished in the next lessons. Please refer to the YouTube videos for more information.

IMPORTANT: you can also watch the videos in English languages:

1. The videos have a English auto dubbed audio track.
2. The Italian subtitles are hand-cleaned and automatically translated English subtitles work great.

Build and run the pi spigot example:

```sh
make
./toyforth examples/pi.toy
```

It prints `3141592653`, the first ten digits including the initial `3`.
Change `10 pi_digits` at the end of the example to request another count.

The example translates the Python spigot algorithm into Toy definitions.
Its state list holds `n`, `a`, `q`, `x`, `nines`, `predigit`, and a flag that
discards the initial dummy digit. `get`, `set`, and `len` access list storage;
`emit` writes character codes. The array stays separate from the operand stack.

Run `make test` to check the first ten digits, other lengths up to 100, and
repeated calls with an empty final stack.
