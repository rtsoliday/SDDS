# SDDS editor

## Build selection

Build the SDDS libraries from the repository root first with `make -j`.
The editor supports Qt 5 and Qt 6. Its code generator (`moc`), headers and
libraries must all come from the same Qt installation.

On Linux and macOS, select the version explicitly when needed:

```sh
make -C SDDSaps/sddseditor QT_MAJOR=5
make -C SDDSaps/sddseditor QT_MAJOR=6
```

The selected Qt installation must be visible to `pkg-config`, including its
Widgets and PrintSupport packages. Set `PKG_CONFIG_PATH` for installations
outside the default search path, and `MOC` if the matching generator is outside
the standard locations. The default preference is Qt 6 on Linux and Qt 5 on
macOS, with the other version used when it is the only installed version.
Qt 6 builds enable C++17. Changing Qt installations rebuilds the editor's
objects and generated header automatically.

On Windows, use the MSVC environment initialized by `build-windows.bat`.
The default kit is `C:/Qt/6.8.2/msvc2022_64`. To build the editor with a
different Qt 5 or Qt 6 MSVC kit, pass its root directory:

```sh
make -C SDDSaps/sddseditor QT_DIR=C:/Qt/5.15.2/msvc2019_64
```

Use a Qt kit compatible with the compiler, and put its `bin` directory on
`PATH` when running the editor or tests so that Windows finds the matching
Qt DLLs. The make recipes also use Cygwin's `cmp` to detect configuration
changes.

## Regression tests

After building the repository, run:

```sh
make -C SDDSaps/sddseditor tests
```

The target builds `SDDSEditorTests.cc` and `PlotSnapshotProbe.cc` with the normal
repository toolchain and runs Qt with `QT_QPA_PLATFORM=offscreen`. Tests exercise
transactional saves (including gzip and xz), version collisions, fixed parameter
insertion, cell and structural undo/redo, sorting, resizing, empty arrays in SDDS
and HDF, numeric row filters, search invalidation, plotting unsaved edits, and
parameter panel sizing when opening files, resizing the window, and manually
dragging splitter handles (including empty array panels).
Column search checks cover all columns with no selection, one or several
selected columns, repeated Enter, case-insensitive matches, filtering and page
changes. Search/replace checks also cover pending edits in highlighted results,
refreshing matches with Next/Previous, and identical replacements preserving
undo/redo history.
Further checks cover copying pending edits, stable sorting of numerically equal
values, consistent placement of invalid numeric cells, refreshing row filters
after column type/name changes, and exact long-double result formatting.

Additional regressions cover pending main-table and array-viewer edits, filtered
search/replace and formula operations, all 256 character bytes and Unicode strings
in plain/gzip/xz saves, signed zero, failed HDF exports and HDF names of `.`.
They also cover literal token text in formulas, embedded NUL rejection, trailing
empty clipboard rows, lossless transfers between the main tables and array
viewers, CSV validation and unequal array lengths, and main-window Undo/Redo
with a pending viewer edit. Further checks cover newlines inside CSV text,
fixed parameters on inserted pages, negative (trimmed) string field lengths in
attribute editors and type changes, and edits that change nothing adding no
Undo step. Name checks cover row filters on bracketed (`[Q[0]]`), case-sensitive
and `i`/`row` column names, and plotting columns whose names contain brackets.
Formula checks cover exact long64/ulong64 fill series and expressions beyond
2^53, and NaN results. Computed float/double values that underflow to zero are
rejected without changing data; representable subnormal values remain valid.
Checks also cover unchanged sorts and array resizes preserving Redo and the
saved state, pending viewer edits on Escape/reject, and subnormal heatmap bounds.
Long-string checks open and commit values beyond 32,767 characters in each table
and the array viewer, then save and reload them. Array action checks cover formula
sequence numbers across unequal array lengths, rectangular keyboard paste from
an active cell editor, and viewer Undo/Redo with pending edits in the main window.
Fixed-parameter checks cover literal backslashes, quotes, whitespace and all 256
character bytes in ASCII/binary and plain/gzip/xz saves, including loading and
resaving escaped definitions. Definition checks reject inserted names containing
NUL, preserve field lengths beyond one million, and keep Undo/Redo and the saved
state when attribute dialogs are accepted without changes.
Editing other parameter attributes also preserves empty fixed strings and fixed
zero-byte characters. Window and encoding checks cover closing the editor while
its search dialog and array viewers are open, HDF export into a directory with a
non-ASCII name, CSV text encoding, array viewer pastes of differently formatted
equal values, attribute edits leaving unchanged definition text untouched, and
the load warning for text the system encoding cannot represent. Shape checks
cover accepting the Resize dialog for a dimension above one million and changing
an array's number of dimensions. Row filter checks reject a stray sign or dot
before an operator, and inserting rows into a document without columns changes
nothing.

Long-text checks also cover editing definitions, fixed values, search replacements
and formula templates beyond 32,767 characters. Staged saves verify that SDDS can
read the generated header before replacing a destination; oversized definitions
are rejected while preserving the file and unsaved edits. Floating-point checks
require exact ASCII and binary round trips in plain/gzip/xz files, using each
platform's native long-double precision (the tests select the library's 64-bit
long-double mode on MSVC and Apple Silicon).

Input failure checks repeatedly open truncated headers, invalid modes, conflicting
byte-order declarations and broken includes in plain/gzip/xz files. They verify
that streams are released and the current document is preserved. The xz checks
also cover blank page separators, final lines without a newline, bounded line
reads and end of file. Integer-expression checks reject floating-point fallback
that would round a large operand, literal, intermediate or fractional result into a valid
integer, and retain exact `floor`/`ceil` operations and ordinary small formulas.
Fill Series also rejects integer calculations that would silently round away a
fraction, while retaining exact series up to the unsigned 64-bit maximum.
Formula and row-filter parsers bound recursive nesting to 256 levels so excessive
parentheses, unary operators or chained powers produce an error instead of
exhausting the process stack. Regressions cover million-level nested input and
ordinary expressions with functions and operator precedence.

Executables are built under the platform object directory (`O.Linux-x86_64` on
Linux). The plotting probe is named `test-bin/sddsplot` there; only the test
process prepends that directory to its PATH. It copies the plot input and records
arguments, so the test verifies the real process launch without opening a plot
window. It does not replace the installed `sddsplot`.

The test prints each result and its fixture directory. Fixtures are retained in
`O.*/test-artifacts-*` for inspection, and the normal `make clean` target removes
them with the object directory. Versioned symlink checks run on Unix platforms,
and on Windows when the account can create symbolic links (Developer Mode or an
elevated prompt); otherwise they are reported as skipped.

The editor stages SDDS saves, CSV exports and HDF exports beside the destination before
replacing it. Failed saves and exports preserve the existing file. Versioned
symlink saves select an unused version
and create it exclusively before replacing the link. On Windows the replacement
is a native symbolic link (not a `.lnk` shortcut), which needs the same permission. Plot snapshots use a private
temporary directory retained until the plotting process ends.

When column rows or array elements are present, the parameter panel initially
fits the height of its rows and header. Extra vertical space goes to the populated
column and array panels. All panels remain manually resizable, including panels
without definitions. Parameter-only data can use the available height.

To include a read-only panel layout check with an existing SDDS file, run:

```sh
SDDSEDITOR_LAYOUT_INPUT=/path/to/file.sdds make -C SDDSaps/sddseditor tests
```

This checks that the panels fill the splitter when loading before or after the
window is shown and when resizing it. Screenshots are retained with the test
artifacts; the supplied file is never saved or modified.

## Interface

The toolbar holds Open, Save, Undo/Redo, page navigation (previous/next arrows
and a page list), Filter rows, Plot (current column), Array viewer, and the
ASCII/Binary save format. Parameters, columns and arrays are shown in separate
panels. Click a panel title to collapse or expand it, or use the **View** menu.
Each panel header shows its count and common actions (Insert and Attributes;
Attributes edits the definition of the current cell's parameter, column or array).

Floating-point values are shown with the fewest digits that convert back to
exactly the same number (0.1, not 0.10000000000000001). Rows hidden by the row
filter are never changed: copy, paste, delete, search/replace, fill and formula
operations act only on visible rows, and a paste fills successive visible rows. Resizing an
array keeps each element at its indices, and so does changing its number of
dimensions in the attribute editor: added dimensions have length 1, and removing
dimensions keeps the elements whose removed indices are 0. Delete clears the
selected cells; on macOS the Delete (backspace) key does too. Export HDF writes `/` and `%` in SDDS
names as `%2F` and `%25`, because HDF5 uses `/` as a path separator. A name of
`.` is written as `%2E`, because HDF5 reserves it for the current group. Column and
array headers have an **Attributes...** menu item, so definitions without rows
can still be edited.

Character fields hold a single byte, displayed as Latin-1. Characters outside
that range must use a string field. String data and CSV exports use the system's
local 8-bit encoding with both Qt 5 and Qt 6. If a file contains text that this
encoding cannot represent (for example Latin-1 bytes on a UTF-8 system), loading
it shows a warning: those characters appear as replacement characters and a save
stores them that way. Attribute dialogs rewrite only the fields you change. Floating-point formatting preserves negative
zero as `-0`. String fields and definition text reject embedded NUL characters,
which the SDDS text format cannot preserve.

ASCII saves retain enough digits to recover the exact floating-point value.
Long text can be edited without truncation, but SDDS header limits still apply
to definitions and fixed values. A save that would produce an unreadable header
leaves the destination intact and reports how to shorten the definition or use
a non-fixed string parameter.

Copying a non-contiguous selection copies the rectangle around it. Other
programs receive empty fields for the unselected cells. Pasting inside the
editor or an array viewer leaves the matching target cells unchanged.
Copy commits pending cell edits before reading the selection. Numeric sorts
preserve the original order of equal values, with NaN and invalid numeric text
after valid numbers in either direction. Column definition changes immediately
refresh an active row filter; renaming a referenced column disables that filter
and shows all rows.

Row filters match column names case-sensitively first, then ignoring case.
Write a name in brackets when it is not a plain identifier, including names
that contain brackets (`[Q[0]] > 0`); a bracketed name always refers to a
column, even one named `row`, `i`, `true` or `false`. Fill Series and numerical
expressions using integer literals, +, -, *, exact division, abs, floor and ceil
compute integer cells with exact 64-bit arithmetic. Other expressions use the
platform's floating-point arithmetic; integer destinations reject fallback when
a large operand or literal would lose precision, or when an intermediate or result is too large
to distinguish integers from fractions. This keeps long64 and ulong64 values
from silently rounding on MSVC and Apple Silicon. Expressions that produce NaN
or infinity store `nan`, `inf` or `-inf` in floating-point cells.

The parameter table lists Name, Type, Units, Value and Description. Only the
value can be edited; double-click the type to change it, or units/description
to open the attribute editor. Units and Description are hidden when no
parameter defines them. Column and array headers show the name plus the type,
array shape and units. Numeric data is right-aligned.

When a row filter is active, a chip next to the Columns title shows the
expression and visible row count. Click it to edit the filter or × to clear
it. The Columns search box finds text in selected columns, or all columns if
none are selected. Press Enter to move through matches in row order and wrap
back to the beginning. Selecting a search result keeps the original search
scope; selecting cells or columns yourself changes it. Searches ignore case
and skip filtered rows.

The status bar shows the save state, file, page, current cell, row counts and
the last undoable action. Messages that used to appear in the top console are
shown briefly in the status bar and kept in the message log. Open the log with
**Messages** (Ctrl+Shift+L); unread messages are counted on the button.

The editor uses its own light or dark palette, chosen from the desktop palette
at startup (Qt 6.5+ also follows later desktop changes). Icons are drawn in
code, so the editor needs only the Qt Widgets module. The table font is the
first installed of Source Code Pro, JetBrains Mono, Cascadia Mono, Consolas,
Menlo, DejaVu Sans Mono or Liberation Mono, falling back to the system
fixed-width font.

## Multidimensional array viewer

Right-click an array header or cell and choose **Open Array Viewer...**, or use
**Edit → Array → Open Array Viewer...**. Each viewer is a separate, nonmodal
window. It displays the selected array on the main editor's current page.

Choose row and column dimensions above the grid. Choosing an axis already in use
swaps the axes; the stored array is never transposed. Other dimensions have spin
boxes and sliders for selecting slices. All indices start at zero, and the full
coordinate and value appear beneath the grid and in cell tooltips. One-dimensional
arrays use a single column. Empty arrays display their shape and an empty message.

Edits use the existing SDDS type validation and update the main table immediately.
Undo/Redo share the editor's history and keep targeting the same array element
after slice changes or closing the viewer. The existing policy of clearing cell
history on a page switch still applies. Slice indices are visibly clamped when a
new page has smaller dimensions. Deleted or renamed arrays become unavailable;
loading a different document closes its viewers.

Copy/Paste supports rectangular selections, and **Copy slice** copies the current
plane. A paste must fit entirely inside the plane and contain valid values; invalid
pastes make no changes. Each paste is one undo operation. Internal array-viewer
copies preserve tabs and newlines in text cells. **Ctrl+S** saves the SDDS document;
closing a viewer does not discard its edits. Slice CSV export is not included.

Enable **Heatmap** to color numeric array cells while keeping their values visible
and editable. **Current slice** scales colors from the smallest to the largest
finite value in the displayed plane and updates after edits, undo, axis changes,
and page changes. **Fixed range** starts with the current slice's limits; edit
**Min** and **Max** and press **Apply** (or Enter) to set bounds that remain fixed
across slices and pages. Values outside those bounds use the end colors. Limits
accept scientific notation and must be finite, with Min no greater than Max.

The purple-to-yellow legend shows the active bounds. Missing, invalid, NaN and
infinite values appear gray and are excluded from automatic bounds. A constant
slice uses the middle color; an empty or all-nonfinite slice has no automatic
range. String and character arrays keep the ordinary grid even if their contents
look numeric. Heatmap settings affect only the display, not saved data, copied
values, or undo history. Batch edits share one deferred automatic range scan.

The regular `tests` target covers 1D through 4D arrays, axis changes, empty slices,
clipboard handling, shared and structural undo, page changes, file round trips,
and closing/replacing viewer documents. Heatmap tests cover automatic and fixed
scales, exact cell text, edits/undo, empty and nonfinite data, extreme numbers,
page changes, and numeric type gating. Viewer and heatmap screenshots are retained
in the test artifact directory.
