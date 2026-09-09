# SDDS editor regression tests

After building the repository, run:

```sh
make -C SDDSaps/sddseditor tests
```

The target builds `SDDSEditorTests.cc` and `PlotSnapshotProbe.cc` with the normal
repository toolchain and runs Qt with `QT_QPA_PLATFORM=offscreen`. Tests exercise
transactional saves (including gzip and xz), version collisions, fixed parameter
insertion, cell and structural undo/redo, sorting, resizing, empty arrays in SDDS
and HDF, numeric row filters, search invalidation, and plotting unsaved edits.

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
