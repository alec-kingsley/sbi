# Simple Befunge Interpreter (SBI)

A Befunge-98 interpreter, with support for Concurrent Befunge.

The wrapping algorithm is taken from [cfunge](https://github.com/VorpalBlade/cfunge/blob/29e4cfa1cc1f4553bf0e2908f819e913c32dfda8/src/funge-space/funge-space.c#L664) which in turn was attributed to Elliot Hird.

Passes Mycology, except for unimplemented fingerprints.

`=`, `i`, and `o` not implemented.

Currently, only the `BOOL`, `NULL`, `MODU`, and `ROMA` fingerprints are supported.

## Installation

Run `./install.sh` to install the `sbi`. It can be run with `sbi <file name>`

## Testing

After installing, you can test the interpreter.

[Mycology](https://github.com/Deewiant/Mycology) is a standard Befunge test suite.
Run `./test_mycology.sh` to download and run these tests and verify that the output looks as
expected.

