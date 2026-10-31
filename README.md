Toyforth is a small Forth-like interpreter built during my
[YouTube C course](https://www.youtube.com/watch?v=HjXBXBgfKyk&list=PLrEMgOSrS_3cFJpM2gdw8EGFyRBZOyAKY).
The videos include English dubbed audio and translated subtitles.

Build and run:

```sh
make
./toyforth examples/addition.toy
```

Programs run left to right. Signed decimal integers push values onto the
stack; words consume inputs and push results. For example, `10 3 -` leaves
`[7]`. Whitespace and newlines separate words. `;` ends a function definition
and otherwise acts as a separator.

All implemented operators and built-in functions are listed below. Stack
notation is `inputs -- outputs`, with the top of the stack on the right.

| Word | Stack effect | Meaning |
| --- | --- | --- |
| `+` `-` `*` | `a b -- result` | Integer addition, subtraction, multiplication |
| `/` | `a b -- quotient` | Integer division, rounded toward zero |
| `%` | `a b -- remainder` | Integer remainder |
| `=` | `a b -- flag` | Equality: 1 if equal, 0 otherwise |
| `dup` | `a -- a a` | Copy the top item |
| `over` | `a b -- a b a` | Copy the second item |
| `pick` | `... n -- ... item` | Copy the item at depth n; 0 is the top |
| `swap` | `a b -- b a` | Exchange the top two items |
| `drop` | `a --` | Remove the top item |
| `list` | `value count -- list` | Create a list filled with value |
| `get` | `list index -- value` | Read a list element |
| `set` | `value list index --` | Replace a list element |
| `len` | `list -- count` | Get a list's length |
| `emit` | `code --` | Write a byte; `10 emit` prints a newline |

List indices start at zero. Duplicating a list shares its storage.

Define a word with `: name ... ;`, then call it by name. Definitions share
the caller's stack and can call other words:

```text
: cube dup dup * * ;
3 cube
```

This leaves `[27]`. Names contain letters, digits, and underscores, and
start with a letter or underscore.

`start limit do ... loop` consumes the bounds and steps toward the exclusive
limit. `0 5 do i loop` leaves `[0 1 2 3 4]`; `5 0 do i loop` leaves
`[5 4 3 2 1]`. Equal bounds skip the body. Loops can nest: `i`, `j`, and `k`
push copies of the innermost, next outer, and third loop indices, which are
kept separately from the data stack.

Both `flag if A then` and `flag if A else B then` are supported. `if` consumes
the flag: nonzero executes A; zero executes B if present, otherwise it skips A.
`then` closes the conditional. `else` introduces the optional false branch.
Conditionals can nest, following the [Forth convention](https://forth-standard.org/standard/core/IF).

```text
1 if 10 else 20 then
0 if 30 else 40 then
```

This leaves `[10 40]`. The interpreter prints stacks in square brackets.

For a larger example:

```sh
./toyforth examples/pi.toy
./toyforth examples/pi_golf.toy
```

Both examples calculate 30 digits of Pi. Change `30 pi_digits` at the end
of either file to select another count; `10 emit` prints the final newline.
The [Python reference](examples/pi.py) also runs at 30 digits.

Run `bash tests/test_pi.sh` to check both examples at 30 digits.

`pi.toy` follows the Python algorithm. `pi_golf.toy` combines its helper
functions into one, uses `pick` for stack access, and keeps intermediate
values on the stack. This reduces the vocabulary and number of calls.

<!-- pi-usage:start -->

| At 30 digits | pi.toy | pi_golf.toy |
| --- | ---: | ---: |
| Distinct words | 31 | 17 |
| Source word calls | 141 | 101 |
| Runtime word calls | 116,849 | 98,140 |

<!-- pi-usage:end -->

Enable runtime profiling with `-p` or `--profile`:

```sh
./toyforth --profile examples/pi.toy
make profile
```

Profiling reports call counts, total milliseconds, and average microseconds
per word to stderr. Function times include their callees. `make profile`
saves both Pi reports and regenerates the comparison.

Saved reports: [pi.toy](profiling/pi.profile.txt) and
[pi_golf.toy](profiling/pi_golf.profile.txt).
See [profiling/README.md](profiling/README.md) for the full word table,
chart, and methodology.
