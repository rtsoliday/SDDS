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

Executables are built under the platform object directory (`O.Linux-x86_64` on
Linux). The plotting probe is named `test-bin/sddsplot` there; only the test
process prepends that directory to its PATH. It copies the plot input and records
arguments, so the test verifies the real process launch without opening a plot
window. It does not replace the installed `sddsplot`.

The test prints each result and its fixture directory. Fixtures are retained in
`O.*/test-artifacts-*` for inspection, and the normal `make clean` target removes
them with the object directory. Versioned symlink checks run on Unix platforms.

The editor stages SDDS saves beside the destination before replacing it. Failed
saves preserve the existing file. Versioned symlink saves select an unused version
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
