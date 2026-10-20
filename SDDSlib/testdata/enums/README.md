# Shared SDDS6 enum fixtures

Files beginning with `valid-` must be accepted by both C and C++ readers. Files
beginning with `invalid-` must be rejected, including under recovery and projection.
The logical mapping in `valid-escapes.sdds` decodes to a label containing comma,
colon, backslash, quote, newline, carriage return, tab, UTF-8 and a trailing space.
Outer namelist escaping is separate from dictionary escaping. NUL and invalid
UTF-8 are intentional binary bytes in the corresponding malformed fixtures.

These files are inputs, never executables. Generated encodings and logs belong in
`O.<platform>/test-artifacts/enum` and `tests/O.<platform>/enum-baseline`.
