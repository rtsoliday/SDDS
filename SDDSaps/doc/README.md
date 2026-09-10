# SDDS Toolkit Documentation

This directory contains the source files for the SDDS Toolkit documentation.

For answers to common questions, see the [FAQ](FAQ.md).

## sddsplot character charts

The [character chart reference](sddsplot-character-charts/README.md) covers all
32 named fonts and the original Greek and special-symbol sets. Each PNG shows
its glyphs and title/label codes. Open [the gallery](sddsplot-character-charts/index.html)
in a browser, or download the [ZIP](sddsplot-character-charts/sddsplot-character-charts.zip).

The HTML build copies the gallery and images alongside the manual. The sddsplot
manual links to the gallery from its special-characters section. When distributing
the PDF separately, include the `sddsplot-character-charts` directory beside it
so the relative link works.

Normal builds use the supplied images. To regenerate them, install Python 3,
Pillow, and DejaVu Sans / DejaVu Sans Mono, then run `make character-charts`.
See the chart reference for source mappings and maintenance notes.

## Build the manual

To build the documentation, ensure the required LaTeX packages are installed and run:

```bash
make
```
