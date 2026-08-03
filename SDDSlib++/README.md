# SDDS++ C++17 library

`libSDDSpp` is the modern, standalone SDDS++ interface to serial SDDS files. It
does not link to the C `SDDSlib` implementation. The C library remains the
format-behavior reference and the command-line utilities remain interoperable
with files produced here.

The human-facing product name is **SDDS++**. The link and file-safe binary name
is `SDDSpp` (`libSDDSpp.a`, `libSDDSpp.so`, or `SDDSpp.lib`/`SDDSpp.dll`). The
public namespace remains `sdds`, and the preferred header remains `SDDS.hpp`.

## Public interfaces

- `SDDS.hpp` is the preferred SDDS++ API. It provides immutable layouts, typed values,
  projection-aware page streaming, row slices, lazy page indexes, typed
  exceptions, checked conversions, resource limits, custom byte sources and
  sinks, live-file following, layout/page transformations, append, and
  incremental page update operations. `sdds::cppApiVersion` identifies the
  source-level API generation. `sdds::libraryName` identifies the product.
- `SDDS3.h` now selects the deprecated `SDDSFile` method adapter by default.
  Define `SDDS3_ENABLE_UNMODERNIZED_API` before including it only when compiling
  the archived pre-C++17 implementation. The adapter preserves commonly used
  method calls, but deliberately does not preserve the former ABI or public
  data members.

The modern type IDs match current SDDS: `longdouble`, `double`, `float`,
`long64`, `ulong64`, `long`, `ulong`, `short`, `ushort`, `string`, and
`character` (IDs 1 through 11).

## Supported format behavior

- SDDS protocol versions 1 through 5, with the minimum output version selected
  from the layout features
- ASCII and binary data
- row-major and column-major binary pages
- native, little-endian, and big-endian binary encoding
- 32-bit row counts and the 64-bit row-count sentinel
- variable, fixed/recoverable, and ASCII no-row-count pages
- gzip, XZ, and LZMA input/output without shell pipelines
- nested `&include` layouts, flattened when a parsed layout is written again
- associates, fixed parameters, arrays, additional header lines, standard
  input/output, headerless input, page append, and last-page update
- independent parameter, array, and column projection with first/count/stride
  or bounded-last-row selection
- path, borrowed `FILE*`, C++ stream, and caller-owned memory adapters; gzip,
  XZ, and LZMA codecs can wrap custom sequential sources and sinks
- advisory reader/writer locking, disconnect/reconnect with file-replacement
  detection, and nonblocking polling of an updateable binary final page
- checked numeric and unit conversions, row views/masks, dense matrices,
  layout editing, and streaming transformed copies

MPI I/O is intentionally outside this library. Use the C SDDS MPI facilities
for parallel files.

## Example

```cpp
#include "SDDS.hpp"

sdds::LayoutBuilder builder;
builder.addColumn({{"x", {}, "m", {}, {}, sdds::Type::Double}, 0});
auto layout = builder.build();

auto writer = sdds::Writer::create("example.sdds", layout);
writer.beginPage(3);
writer.setColumn("x", std::vector<double>{1.0, 2.0, 3.0});
writer.commitPage();
writer.close();

auto reader = sdds::Reader::open("example.sdds");
sdds::ReadRequest request;
request.parameters = sdds::FieldSelection::noFields();
request.arrays = sdds::FieldSelection::noFields();
request.columns = sdds::FieldSelection::only({"x"});
request.rows.stride = 10;
while (auto page = reader.next(request)) {
  const auto &x = page->columnAs<double>("x");
  // Unrequested fields remain in the layout but throw StateError on access.
}
```

## Demos

The C examples in `SDDSlib/demo` have C++ counterparts in `SDDSlib++/demo`:

- `sddspp_write_demo` writes two ASCII pages containing every SDDS scalar type.
- `sddspp_read_demo` generically prints parameters, arrays, and columns from an
  input file using its immutable layout.
- `sddsppdemo` writes and reads a compact binary file using typed accessors and
  zero-copy row views.

They are built by the repository's top-level `make`, or directly with:

```sh
make -C SDDSlib++/demo
SDDSlib++/demo/O.$(uname -s)-$(uname -m)/sddspp_write_demo example.sdds
SDDSlib++/demo/O.$(uname -s)-$(uname -m)/sddspp_read_demo example.sdds
```

## Build and test

From the repository root:

```sh
make -C SDDSlib++
make -C SDDSlib++ test
make -C SDDSlib++ fuzz-smoke
make -C SDDSlib++ sanitizer-test
make -C SDDSlib++ benchmark
make -C SDDSlib++ benchmark-gate
```

The integration test writes its artifacts below the normal `O.<os>-<arch>`
build directory. The focused and differential tests cover C-to-C++ and
C++-to-C interoperability, versions 1 through 5, all SDDS scalar types,
ASCII/binary data, both byte orders and major orders, compression, projection,
row slicing, custom streams, headerless input, reconnect/locking/live reads,
updates, transformations, malformed input, and the deprecated facade.

The sanitizer target uses Clang AddressSanitizer and UndefinedBehaviorSanitizer.
Leak detection is disabled because monitored build hosts may run under process
inspection that is incompatible with LeakSanitizer. `make fuzz` builds the
opt-in libFuzzer harness. The benchmark gate takes five alternating C and C++
runs and requires at least 85% of C throughput and no more than 125% of C peak
memory for representative uncompressed numeric reads and writes. String-heavy,
array, compression, projection, sparse, seek, append, and update results are
reported but are not hard gates.

See [MIGRATION.md](MIGRATION.md) for the API-generation-2 source migration.
See [BENCHMARKS.md](BENCHMARKS.md) for workload definitions, release thresholds,
and the rationale for non-gated string, array, and compressed results.
