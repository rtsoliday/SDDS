# Native SDDS6 categorical enums

SDDS6 adds `enum` to parameters (including fixed parameters), columns and arrays.
Its type ID is `SDDS_ENUM = 12`; IDs 1–11 are unchanged. Every enum value is a signed
32-bit code with a required, element-local dictionary. Codes need not be contiguous
or nonnegative. Enums are excluded from numeric, integer and floating categories.

Applications and bindings **must be rebuilt together**. Definition structures and
`SDDS_LAYOUT` have changed, `SDDS_NUM_TYPES` is 12, and the three `SDDS_ANY_*`
constants now have fixed values `0x10001`, `0x10002` and `0x10003`. The C++ API
generation is 3. Reusing an application compiled against the old headers is unsafe.

## File contract

```text
SDDS6
&parameter name=expected, type=enum, enum_values="0:OFF,10:ON,-1:FAULT", fixed_value="10" &end
&column name=state, type=enum, enum_values="0:OFF,10:ON,-1:FAULT" &end
&data mode=ascii &end
3
0
10
-1
```

ASCII data and fixed values contain decimal **codes**. Binary data contains exactly
four bytes per code in the declared byte order. Array dimensions and page row counts
keep their existing encodings. Both binary major orders and plain, gzip, XZ and
LZMA streams are supported. SDDS1–5 headers cannot declare `enum` or `enum_values`.
Writers choose SDDS6 whenever any enum exists; enum-free layouts retain the existing
minimum-version selection. Pre-change readers reject SDDS6.

Each enum has exactly one nonempty `enum_values` attribute. Its logical value is a
comma-separated list of `code:label` pairs. Codes and labels must each be unique;
labels must be nonempty, valid UTF-8 without NUL. Matching is case sensitive and
does not normalize Unicode. Declaration order and label whitespace are preserved.
No locale-specific collation or numeric meaning is implied by a code.

The logical dictionary grammar supports these escapes:

| Logical escape | Label character |
| --- | --- |
| `\,` | comma |
| `\:` | colon |
| `\\` | backslash |
| `\"` | double quote |
| `\n`, `\r`, `\t` | newline, carriage return, tab |

Unknown escapes, dangling escapes, raw delimiters within labels, empty labels,
duplicate codes or labels, invalid UTF-8 and codes outside `INT32_MIN..INT32_MAX`
are errors. For example, `0:A,0:B`, `0:A,1:A`, `0:`, `0:A\q`, and
`2147483648:A` are invalid logical mappings.

Apply outer namelist escaping **after** constructing the logical dictionary. For
example, logical `10:a\,b\:c\\d` appears in a file as
`enum_values="10:a\\,b\\:c\\\\d"` and labels code 10 as `a,b:c\d`.
The shared [fixtures](../testdata/enums/README.md) include every escape, UTF-8,
trailing whitespace, integer limits, NUL, malformed mappings and version failures.
SDDS6 has a separate bounded namelist parser; SDDS1–5 retain their legacy parser.

Zero initialization continues to produce code zero. It is valid only when the
dictionary explicitly maps zero. Neither library substitutes the first state.
An unset enum parameter or column therefore fails validation if zero is unmapped.

## C API and ownership

Use `SDDS_DefineEnumColumn`, `SDDS_DefineEnumParameter` or `SDDS_DefineEnumArray`.
Their arguments follow the corresponding primitive definers, replacing `type`
with the logical dictionary string. Use a NULL `format_string` and zero
`field_length`. Fixed enum parameters take a decimal code string. Existing
primitive definers reject `SDDS_ENUM` and direct callers to these functions.

```c
SDDS_DefineEnumColumn(&dataset, "state", NULL, NULL, NULL, NULL,
                      "0:OFF,10:ON,-1:FAULT", 0);
int32_t codes[] = {0, 10, -1};
SDDS_SetColumn(&dataset, SDDS_SET_BY_NAME, codes, 3, "state");
```

Check every return value in application code. Scalar, bulk and array assignments
validate membership. Readers validate codes, including discarded sparse rows;
whole-page writers revalidate direct C buffer changes before serialization.
Context-free numeric getters, casts and presentation formatters reject enums.
The low-level ASCII wire encoder/decoder uses integer codes; label presentation
uses the dictionary-aware `SDDS_SprintEnumValue` instead.

| API | Ownership / behavior |
| --- | --- |
| `SDDS_CreateEnumDefinition`, `SDDS_FreeEnumDefinition` | Allocate/free an independent immutable dictionary |
| `SDDS_EnumValues`, `SDDS_EnumCodeToLabel`, `SDDS_EnumEntry` | Borrowed read-only text; valid for dictionary lifetime |
| `SDDS_EnumLabelToCode` | Returns success separately and writes an `int32_t`, including negative codes |
| `SDDS_EnumCount` | Cardinality; entries are enumerated in declaration order |
| `SDDS_EnumDefinitionsEqual` | Compares code/label associations, independent of declaration order |
| `SDDS_RemapEnumValues` | Explicit label-based remapping, validating the complete input before changing output |
| `SDDS_ConfigureEnumConversion` | Configure enum/long/string conversion in a fresh output layout |
| `SDDS_CopyPageWithEnumConversions` | Copy into that layout with checked conversion/remapping |
| `SDDS_CheckEnumFieldCompatibility` | Check dictionaries for selected or renamed ordinary copies |
| `SDDS_CheckEnumExport` | Reject implicit dictionary loss in primitive-only formats |

`enum_values` is available from all three existing information-query APIs. As
with other queried string fields, the returned string is an allocated copy that
the caller frees. Definition dictionaries own their canonical text; do not free
or modify definition members directly. Working and saved layouts share ownership
within a dataset. Definition-copy and dataset-copy APIs create independent
dictionaries; use the matching existing free/terminate API for those copies.

Dictionaries cannot be replaced through generic information mutation. Enum
formats and field widths cannot be changed through generic mutation either. Once data
storage exists or a layout has been written, dictionary changes require a new
output layout and explicit checked conversion. Ordinary copy, transfer, append,
combine and renamed copy paths require equal dictionaries when enums are involved.
Legacy in-place parameter deletion with enums reports an unsupported operation;
construct a new layout instead. The already-unavailable in-place column deletion
API remains unavailable. Enum data copies within one dataset retain independent
field definitions and dictionary ownership. Conversion failures can leave an unfinished output; callers should use their
usual temporary-output/rename workflow when transactional file replacement matters.
`SDDS_ConfigureEnumConversion` requires layout saving to be enabled. A call while
`SDDS_DeferSavingLayout` is active fails before changing the layout; finish and save
the deferred changes before configuring conversions.

Default limits are 16 MiB per SDDS6 layout command and 1 MiB per decoded logical
mapping (including its canonical form). `SDDS_SetEnumReaderLimits` sets limits for
the calling thread; `SDDS_GetEnumLayoutLimit` and `SDDS_GetEnumMappingLimit` query
them. Limits must be positive, the mapping limit cannot exceed the command limit,
and the command limit must be below `INT32_MAX`. Allocation sizes are checked.
Sorted indices make code and label lookup logarithmic in dictionary cardinality.

## C++ API generation 3

`sdds::Type::Enum` uses a distinct `sdds::EnumValue{int32_t code}` alternative in
both scalar and vector variants. `FieldMetadata::enumDefinition` holds a
`shared_ptr<const EnumDefinition>`. Builders validate dictionary presence, fixed
codes, format/field-length restrictions and duplicate element names. Type mapping
is explicit; consumers must not infer protocol IDs from variant positions.

```cpp
auto states = std::make_shared<const sdds::EnumDefinition>("0:OFF,10:ON,-1:FAULT");
sdds::ColumnDefinition state;
state.name = "state";
state.type = sdds::Type::Enum;
state.enumDefinition = states;
auto layout = sdds::LayoutBuilder().addColumn(state).build();
sdds::Page page(std::make_shared<const sdds::Layout>(layout));
page.setColumn("state", std::vector<sdds::EnumValue>{{0}, {10}, {-1}});
```

`EnumDefinition::label` and `code` provide checked lookup. `convertEnumScalar` and
`convertEnumValues` explicitly support Enum↔Int32, Enum↔String and label-based enum
remapping; enum targets require a target dictionary. `remapEnum` handles one code.
Type and dictionary requirements apply even to empty bulk conversions.
The ordinary numeric conversion API rejects enums. `ReaderLimits` exposes
`maxLayoutCommandBytes` and `maxEnumDefinitionBytes`. Projection still validates
discarded enum data; skipped ASCII enum arrays are validated without retaining
tokens for the entire array. Page setters, writers, filtered/projected pages, append and
last-page updates retain dictionary meaning.

The maintained `SDDS3Legacy.h` adapter reads/writes SDDS6, exposes complete
dictionaries through `getColumnEnumValues`, `getParameterEnumValues` and
`getArrayEnumValues`, displays column labels through `getColumnInString`, and provides
`defineEnumColumn`, `defineEnumParameter` and `defineEnumArray`. Its integer setters
are checked code setters; numeric extraction still rejects enums. The archived
implementation selected by `SDDS3_ENABLE_UNMODERNIZED_API` remains unsupported
for SDDS6. External Python bindings, separately maintained Fortran `_F` adapters,
Java/Tcl/Matlab consumers and other independently built applications require their
own coordinated update/rebuild. `SDDSFortran.h` includes the new constants, and
`SDDSenum.f90` supplies ISO_C_BINDING interfaces using C-owned dataset handles.

## Toolkit and editor workflows

```sh
sddsmakedataset states.sdds -column=state,type=enum -data=0,10,-1 \
  '-enumValues=column,state,0:OFF,10:ON,-1:FAULT'
sddsquery states.sdds
sdds2stream states.sdds -column=state -noquotes
sdds2stream states.sdds -column=state -enumCodes
sddsprintout states.sdds -column=state
sddssort states.sdds sorted.sdds -column=state
sddsprocess states.sdds on.sdds -match=column,state=ON
```

`sddsmakedataset` supports the repeatable `-enumValues=kind,name,mapping` option
for columns, parameters and arrays, and takes integer codes as data. Appending
requires the same mapping. The first two option separators are structural;
the remainder is parsed using the dictionary grammar, preserving escaped commas.

Sorts compare labels in UTF-8 byte order; equality/differences require compatible
mappings. Numeric/absolute sorts, arithmetic, statistics, interpolation and numeric
extraction of selected enums require explicit conversion. Unrelated enum fields
survive ordinary layout/page copies during numeric processing. String row matching
uses labels, as do enum row labels in `sddsdiff`. `sddscollect` and `sddsseparate`
require equal dictionaries for every column combined into the same output field;
convert or remap incompatible columns explicitly before grouping them.
`sddscombine -collapse`, `sddschanges -baseline -copy`, and
`sddsgroupedenvelope` also require equal dictionaries for enum values copied
between files. Equal mappings may list their entries in different orders.
Editor row filters compare enum labels as text, so `"01"` and `"1"` remain
distinct states even when both labels look numeric.
RPN row/parameter memories expose labels; numeric RPN column arrays
exclude enums. RPN access rejects an enum whose name already belongs to a numeric
RPN memory in the same process; shared RPN namespaces still require caller discipline. Stored labels are borrowed for the
dataset lifetime, as with existing RPN string values.

Downgrade or remap explicitly:

```sh
sddscast states.sdds codes.sdds -cast=column,state,enum,long
sddscast states.sdds labels.sdds -cast=column,state,enum,string
sddscast labels.sdds remapped.sdds -cast=column,state,string,enum \
  '-enumValues=column,state,7:OFF,8:ON,9:FAULT'
sddscast states.sdds remapped.sdds -cast=column,state,enum,enum \
  '-enumValues=column,state,7:OFF,8:ON,9:FAULT'
```

Use `parameter` or `array` for other element kinds. Converting to enum requires a
target mapping; missing labels or unmapped codes are errors. Further numeric
conversion goes through an explicit ordinary `long` conversion. Primitive downgrade
inherits legacy reader header-size limits. For long fixed-label exports, first use
`sddsprocess input.sdds varying.sdds` to make fixed parameters page data, then
cast the resulting parameters to strings; ordinary long labels belong in data
rather than oversized SDDS1–5 fixed headers. Primitive-only
exporters reject enum input and direct users to `sddscast` first. The editor shows
`label (code)`, edits values through validated state choices, preserves mappings on
save, and replaces dictionaries through a new layout with checked label conversion.
CSV/HDF editor export also requires explicit primitive conversion.
`sddsprintout` prints long SDDS6 element names, but its legacy `editLabel` editor
accepts input labels shorter than 1024 bytes. Use `label=<short-label>` with
`editLabel` when an element's name exceeds that limit.

## Supported-operation inventory and evidence

The following inventory assigns affected entry points a defined behavior. Toolkit
commands that have no categorical implementation retain primitive-only selected
operations and report a type error; introducing `enum` does not turn their numeric
work into code arithmetic. Third-party codec/numerical implementations are unchanged.

| Paths / entry points | Outcome | Verification |
| --- | --- | --- |
| `include/SDDStypes.h`, `SDDS.h`; `SDDS_data.c` | Stable primitive IDs; categorical ID, metadata and constants | C enum tests; coordinated full rebuild |
| `SDDS_enum.c`, shared `SDDSenum.h` | Parse, own, index, compare, lookup, validate, format, remap | C/C++ shared fixtures; lookup scaling; sanitizer tests |
| `SDDS_output.c`, `SDDS_input.c`, `SDDS_process.c`, `SDDS_write.c` | SDDS6 header/version routing; bounded separate escaping; old versions preserved | C/C++ differential matrix; malformed/version fixtures; pytest legacy-reader check |
| `SDDS_ascii.c`, `SDDS_binary.c` | Codes in every element kind, compression/order path; validate sparse/skipped input and raw writes | C enum matrix, sparse/last-row checks; C++ differential tests |
| `SDDS_dataprep.c`, `SDDS_extract.c`, `SDDS_rpn.c` | Checked assignments and label extraction/matching; reject numeric extraction; RPN labels | C setter tests; toolkit numeric rejection/matching/preservation tests |
| `SDDS_copy.c`, `SDDS_transfer.c`, `SDDS_utils.c`, `SDDS_info.c` | Independent cloning, saved-layout lifetime, equal-map ordinary copies, immutable mapping/type | C copy/transfer/query tests; toolkit combine conflicts; editor save and mapping edits |
| `SDDSLayout.cc`, `SDDSModern.cc`, `SDDSTransform.cc`, `SDDSLegacy.cc`, `SDDS.hpp`, `SDDS3Legacy.h` | Native enum values/dictionaries; checked conversions, projection, transforms, append/update; maintained adapter | C++ regular, advanced, differential, enum and sanitizer targets; shared fuzz corpus |
| `SDDSmpi_enum.c`, `SDDSmpi_input.c`, `SDDSmpi_output.c`, `SDDS_MPI_binary.c` | Length-prefixed dictionaries, rank-wide validation/errors, endian-safe data; complete serial master-reader broadcasts | Named two-rank MPI enum target with invalid values/mappings, compressed master reads, repeated/empty pages and collective EOF/errors |
| `sddsmakedataset`, `sddscast` | Creation, explicit codes/labels/remapping, checked append | Toolkit enum pytest suite |
| `sddsquery`, `sddscheck`, `sdds2stream`, `sddsprintout` | Complete mapping query/check; label display and explicit code display | Toolkit shared fixtures and display tests |
| `sddsconvert`, `sddscombine`, core-based copies and filters | Preserve dictionaries; reject incompatible ordinary copies, including collapsed parameters | Toolkit round trips, combine conflicts, collapse mapping regressions, matching and numeric passthrough tests |
| `sddschanges -baseline -copy`, `sddsgroupedenvelope` | Require compatible dictionaries for copied baseline columns, grouping parameters and copied group columns | Compatible/reordered and incompatible cross-file enum regressions |
| `sddssort`, `sddsdiff` | Label sorting, compatible categorical equality and enum row labels; reject absolute enum sorting | Toolkit sort/diff tests; comparison dispatch checks |
| `sddscollect`, `sddsseparate` | Preserve grouped enum values and require equal dictionaries for all members | Compatible/reordered and incompatible group regression tests |
| `sddsxref`, `sddsmxref`, `sddsselect` | Primitive match/equate keys remain type checked; preserve copied enums and require equal mappings, including renamed copies | Selected-key type checks; per-field compatibility guards; existing utility pytest suites |
| Other numeric toolkit selections, type-specific structural transforms | Numeric classification/getters/casts reject enums; core copy/transfer paths preserve unrelated enum metadata; unsupported selected definitions reject through primitive definers | Shared type dispatch and full build; existing toolkit suite; explicit selected-enum rejection tests |
| `sdds2headlessdata`, `sdds2plaindata`, `sdds2spreadsheet`, `sdds2math`, `sdds2mpl`, `sdds2tiff`, `sdds2stl`, `sdds2agilentArb`, `utils/sdds2hdf` | Reject dictionary loss before export; require explicit primitive conversion | Shared export preflight; toolkit export regression |
| `sddseditor` models, delegates, attributes, snapshots, write/reopen, CSV/HDF export | Labels/codes, validated choices, exact enum label filters, fresh-layout dictionary edits, undo and preservation | `make -C SDDSaps/sddseditor tests` using Qt offscreen |
| `SDDSFortran.h`, `SDDSenum.f90`; external adapters | Updated constants and C interoperability interfaces; external wrappers require coordinated updates | Fortran interface compile; documented external release dependency |

## Risks and release qualification

| Risk | Control / remaining obligation |
| --- | --- |
| Old ABI, type IDs used as array indices, stale bindings | Rebuild every library, application and binding; audit external fixed-size tables and variant-index assumptions |
| Older readers misinterpret a new primitive | Mandatory SDDS6 and explicit rejection under SDDS1–5; retain pre-change reader compatibility checks |
| Escaping ambiguity, malformed UTF-8, duplicate/unknown mappings | Separate bounded SDDS6 grammar, shared valid/invalid fixtures, unique indexed dictionaries |
| Header/resource exhaustion, overflow, large dictionaries | Configurable 16 MiB/1 MiB limits and checked allocation/payload lengths; benchmark lookup scaling |
| Leaks, double frees, dangling saved-layout metadata | Single owning dictionaries and independent clones; termination, snapshot and sanitizer coverage |
| Unmapped zero/defaults, sparse negative codes, narrowing | Exact int32 parsing and membership checks; no first-state substitution; validate setters, reads and raw writes |
| Same code has different meanings across files | Equal-map guards on ordinary copies/appends/joins; explicit label remapping |
| Silent numeric treatment or metadata loss | Numeric category exclusion, contextual conversions, selected-operation errors, export preflight and preservation tests |
| Unicode ordering or spelling surprises | Exact UTF-8 matching without normalization; bytewise label sort; display both label and code in editor |
| Different encodings disagree | C↔C++ and serial↔MPI checks, major/byte-order and compression matrices |
| MPI allocation or validation failure strands other ranks | Length-prefixed payloads and collective validation/status propagation; single-rank error regressions |
| Partial repository or external rollout | Require complete repository checks and a coordinated external rebuild before production release; qualify other supported hosts/toolchains separately |
| Regressions or unexpected cost on enum-free files | Existing primitive suite and minimum-version selection; numeric benchmark gate |

Run commands separately from the repository root:

```sh
make clean
make -j
make test
make -C SDDSlib -f Makefile.mpi enum-test
make -C SDDSaps/sddseditor tests
make -C SDDSlib enum-sanitizer-test
make -C SDDSlib++ sanitizer-test
make -C SDDSlib++ fuzz-smoke
make -C SDDSlib++ benchmark-gate
```

Parallel MPI file I/O requires an MPI-capable host and uncompressed binary files;
use serial SDDS conversion before/after parallel file I/O for compression. The
`SDDS_Master_InitializeInput` / `SDDS_Master_ReadPage` API reads serially on rank
zero and broadcasts complete pages, including enum parameters, arrays and columns;
it also supports compressed input and ASCII data. All ranks must participate in
each read, including EOF and error returns. SDDS6 broadcasts use bounded,
length-prefixed commands for all metadata, including ordinary long fields. Sanitizer targets use normal build directories;
Linux GCC builds link sanitizer runtimes statically. They use no injected libraries
or executable wrappers. Existing sanitizer policy disables leak detection on
monitored hosts. Executables, generated fixtures and logs belong in repository
`O.<platform>` build/test-artifact directories; do not commit them.

The pre-change reader binaries and qualification logs for this implementation are
in `tests/O.Linux-x86_64/enum-baseline/`. The pre-change Python baseline was
868 passed, 1 skipped, and one existing RPN thread-test failure caused by C shell startup output. Keep the
baseline readers/logs outside component build directories so `make clean` retains
them. Qualification results and any remaining host-specific limits must be reviewed
alongside these source changes before a production release.

## Qualification on Linux-x86_64, 2026-10-19

The final source passed `make clean` followed by `make -j` with GCC/G++ 11.3.0 and
no compiler warnings. The following checks were run after that rebuild:

| Check | Result | Log under `tests/O.Linux-x86_64/enum-baseline/` |
| --- | --- | --- |
| Root `make test`, including C/C++ enum and differential tests | Native tests passed; pytest: 916 passed, 1 skipped, 1 existing failure | `test.log` |
| Two-rank MPI enum target | Passed, including both byte/major orders and collective invalid-input checks | `mpi-test.log` |
| Qt offscreen editor regressions | All passed | `editor-test.log` |
| C enum address/undefined sanitizer target | Passed | `c-sanitizer.log` |
| C++ advanced and enum address/undefined sanitizer target | Passed | `cpp-sanitizer.log` |
| C++ fuzz smoke with shared enum fixtures | Passed | `fuzz-smoke.log` |
| Enum-free numeric benchmark gate | Passed | `benchmark-gate.log` |
| Fortran ISO C binding interface compile with warnings enabled | Passed | `fortran.log` |

Pytest used the existing `PYTEST` override to keep its generated helpers inside
the repository:

```sh
make test PYTEST='pytest -q --basetemp=tests/O.Linux-x86_64/enum-baseline/pytest-final'
```

The failing test is
`tests/test_rpn_thread_safety.py::test_rpn_cshs_is_thread_safe_without_signal_handlers`.
The host's C shell startup prints `purify`, `purecov` and `quantify` path messages
before the expected `csh-thread-safe` output. This is the same failure recorded
before implementation (868 passed, 1 skipped, 1 failed); the test was not weakened
or excluded. Root `make test` therefore still returns a failure status on this host.

The benchmark gate measured C++/C read throughput at 0.997 and write throughput
at 1.024, above the 0.85 threshold; peak memory ratios were 0.987 and 0.983,
below the 1.25 threshold. Indexed dictionary tests performed 200,000 lookups at
1,000 and 10,000 states without linear growth in lookup time. These measurements
qualify this host's existing gate, not a guarantee for other platforms.

Preserved pre-change reader SHA-256 identities are:

```text
9057e9a13590e21508567bdb5b569575628ae108bfb33614566f9f0b0a974720  sddsquery
dc5eaaf0b9d596ef461369d1d970251f421d3705a3805b3d54b3430d316f0a78  sdds2stream
94f8bdd53a81406840f6d8aeac977803bc6192ef9c3c29fbde96f53c76de3aed  sddspp_read_demo
```

Compatibility tests confirmed all three reject SDDS6 enum input. The supported
operation inventory above has no unresolved repository item. Production release
still requires review of the existing host failure, qualification on other supported
platforms, and coordinated updates/rebuilds of external consumers and bindings.
