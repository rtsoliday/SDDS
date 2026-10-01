# SDDS editor regression tests

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

Additional regressions cover pending main-table and array-viewer edits, filtered
search/replace and formula operations, all 256 character bytes and Unicode strings
in plain/gzip/xz saves, signed zero, failed HDF exports and HDF names of `.`.
They also cover literal token text in formulas, embedded NUL rejection, trailing
empty clipboard rows, lossless transfers between the main tables and array
viewers, CSV validation and unequal array lengths, and main-window Undo/Redo
with a pending viewer edit. Further checks cover newlines inside CSV text,
fixed parameters on inserted pages, negative (trimmed) string field lengths in
attribute editors and type changes, and edits that change nothing adding no
Undo step.

Executables are built under the platform object directory (`O.Linux-x86_64` on
Linux). The plotting probe is named `test-bin/sddsplot` there; only the test
process prepends that directory to its PATH. It copies the plot input and records
arguments, so the test verifies the real process launch without opening a plot
window. It does not replace the installed `sddsplot`.

The test prints each result and its fixture directory. Fixtures are retained in
`O.*/test-artifacts-*` for inspection, and the normal `make clean` target removes
them with the object directory. Versioned symlink checks run on Unix platforms.

The editor stages SDDS saves, CSV exports and HDF exports beside the destination before
replacing it. Failed saves and exports preserve the existing file. Versioned
symlink saves select an unused version
and create it exclusively before replacing the link. Plot snapshots use a private
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
array keeps each element at its indices. Export HDF writes `/` and `%` in SDDS
names as `%2F` and `%25`, because HDF5 uses `/` as a path separator. A name of
`.` is written as `%2E`, because HDF5 reserves it for the current group. Column and
array headers have an **Attributes...** menu item, so definitions without rows
can still be edited.

Character fields hold a single byte, displayed as Latin-1. Characters outside
that range must use a string field. Floating-point formatting preserves negative
zero as `-0`. String fields and definition text reject embedded NUL characters,
which the SDDS text format cannot preserve.

Copying a non-contiguous selection copies the rectangle around it. Other
programs receive empty fields for the unselected cells. Pasting inside the
editor or an array viewer leaves the matching target cells unchanged.

The parameter table lists Name, Type, Units, Value and Description. Only the
value can be edited; double-click the type to change it, or units/description
to open the attribute editor. Units and Description are hidden when no
parameter defines them. Column and array headers show the name plus the type,
array shape and units. Numeric data is right-aligned.

When a row filter is active, a chip next to the Columns title shows the
expression and visible row count. Click it to edit the filter or × to clear
it. The Columns search box finds the next match in the current column.

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
