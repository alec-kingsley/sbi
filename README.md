# Simple Befunge Interpreter (SBI)

A Befunge-98 interpreter, with support for Concurrent Befunge.

Passes most of Mycology with a few exceptions:
- wraparounds with non-cardinal deltas don't work
- some parts of `y` instruction are not yet implemented
- There is an infinite loop at the conditional `_` at `204:52`. Replacing it with `>` allows it to proceed. I have not yet debugged why this is.
- The special character at `199:3` is interpreted as 2 characters, which breaks the interpreter. It must be replaced with a single character, however doing so with any normal character causes it to not pass the test.

`=`, `i`, and `o` not implemented.

Currently, only the `BOOL`, `NULL`, and `ROMA` fingerprints are supported. Mycology claims that these
do not push the correct fingerprint number, however I have verified the number is correct against
another interpreter. (Test code: `"LLUN"4($.a,@` should print `1314212940`)

## Installation

Run `./install.sh` to install the `sbi`. It can be ran with `sbi <file name>`

## Testing

After installing, you can test the interpreter.

[Mycology](https://github.com/Deewiant/Mycology) is a standard Befunge test suite.
Run `./test_mycology.sh` to download and run these tests and verify that the output looks as
expected.

As noted above, you may wish to edit `./Mycology/mycology.b98` to display further tests.

Replace the character at `208:52` with `>`, and the character at `199:3` with any 1-byte printable character (although note that it will result in a failed test)
