# sddsplot JSON output schema (draft)

This document defines the initial JSON schema for `sddsplot -device=json`. The goal is to map SDDS plot requests to a Plotly-friendly structure while preserving SDDS metadata.

## Top-level structure

```json
{
  "schemaVersion": "sddsplot-json-1",
  "generator": {
    "name": "sddsplot",
    "version": "11",
    "date": "YYYY-MM-DD"
  },
  "meta": {
    "command": "sddsplot ...",
    "device": "qt",
    "font": "rowmans"
  },
  "plots": [
    {
      "pageIndex": 0,
      "panelIndex": 0,
      "panelId": "p0-0",
      "layout": {
        "title": "...",
        "topline": "...",
        "xaxis": {
          "title": "...",
          "units": "...",
          "type": "linear",
          "range": [xmin, xmax]
        },
        "yaxis": {
          "title": "...",
          "units": "...",
          "type": "linear",
          "range": [ymin, ymax]
        },
        "legend": {
          "show": true
        }
      },
      "traces": [
        {
          "name": "dataset label",
          "type": "scatter",
          "mode": "lines",
          "x": [1.0, 2.0, 3.0],
          "y": [4.0, 5.0, 6.0],
          "meta": {
            "requestIndex": 0,
            "datasetIndex": 0,
            "fileIndex": 0,
            "page": 1,
            "xColumn": "x",
            "yColumn": "y",
            "xUnits": "s",
            "yUnits": "m"
          },
          "style": {
            "line": {
              "width": 1,
              "dash": "solid",
              "color": "#000000"
            },
            "marker": {
              "size": 6,
              "symbol": "circle",
              "color": "#000000"
            }
          }
        }
      ]
    }
  ]
}
```

## Mapping notes

- `plots[]` contains one entry per rendered panel. This keeps multi-page, multi-panel output intact.
- `layout.*` is Plotly-compatible. Axis ranges are optional; if not specified by `sddsplot`, omit `range` and let Plotly auto-range.
- `traces[]` corresponds to SDDS datasets after all transforms (sampling, clipping, splitting, transpose, etc.).
- `meta.*` carries SDDS-specific information to support downstream filtering or labeling in a client app.
- `style` captures line/marker settings derived from `-graphic`, `-linetypeDefault`, and thickness settings.

## Field definitions

### `schemaVersion`
String identifier for compatibility. Initial value: `sddsplot-json-1`.

### `generator`
- `name`: fixed to `sddsplot`.
- `version`: sddsplot version string from the program header.
- `date`: generation date.

### `meta`
Global metadata such as the command line, device, font, and relevant flags.

### `plots[]`
Each panel has:
- `pageIndex`: zero-based page index (after any `-fromPage`, `-toPage`, `-usePages`).
- `panelIndex`: zero-based panel index within page.
- `panelId`: stable identifier (e.g., `p<page>-<panel>`).
- `layout`: axis titles, units, and plot titles.
- `traces`: list of dataset traces for the panel.

### `traces[]`
- `name`: label used in legend.
- `type`: Plotly trace type (initially `scatter` for 2D x-y plots).
- `mode`: `lines`, `markers`, or `lines+markers` derived from plot element types.
- `x`, `y`: numeric arrays.
- `meta`: SDDS source metadata.
- `style`: Plotly line/marker style information.

## Supported plot types (initial scope)

- 2D x-y plots (`scatter`) with line/symbol styles.
- Error bars and 3D plots are excluded from the initial JSON schema and will be added later.

## Backward compatibility

The JSON output is only emitted when `-device=json` is requested. All other output modes remain unchanged.
