# sddsplot character charts

[Open the gallery](index.html) in a browser, or [download all 34 PNG charts](sddsplot-character-charts.zip).
The images below can also be opened directly in a repository viewer.

Each cell shows a glyph and the literal title/label text needed to produce it.
For fonts without an inline switch, use the `-font=name` option shown on the chart.
`$r` restores your configured default font; `$e` ends the original special-symbol mode.
Charts assume normal text mode on entry. Amber dots mark glyphs taken from the
legacy symbol table, which can override the selected font. Glyphs are fitted
individually for legibility, not shown at their relative typographic sizes.

## Using the codes

In Bash, single-quote the title to preserve dollar signs:

```bash
sddsplot data.sdds -columnNames=x,y '-title=Angle $ga$r'
```

With double quotes, escape each dollar sign:

```bash
sddsplot data.sdds -columnNames=x,y "-title=Angle \$ga\$r"
```

For a named default font:

```bash
sddsplot data.sdds -columnNames=x,y -font=timesr '-title=Example'
sddsplot -listFonts
```

In other scripting languages, pass the title as one argument with dollar signs
unchanged. The chart codes are text fragments, not complete shell commands;
commas and quotes also need protection appropriate to the sddsplot option parser.
A space renders blank. Literal `~` and backslash are unsupported by the character
mapping. **`$y3$r` draws an arc, not a tilde.**

## Charts

| Character set | Selection | PNG |
|---|---|---|
| astrology | `-font=astrology` | [Chart](astrology.png) |
| cursive | `-font=cursive` | [Chart](cursive.png) |
| cyrilc_1 | `-font=cyrilc_1` | [Chart](cyrilc_1.png) |
| cyrillic | `$c...$r` or `-font=cyrillic` | [Chart](cyrillic.png) |
| futural | `-font=futural` | [Chart](futural.png) |
| futuram | `-font=futuram` | [Chart](futuram.png) |
| gothgbt | `-font=gothgbt` | [Chart](gothgbt.png) |
| gothgrt | `-font=gothgrt` | [Chart](gothgrt.png) |
| gothiceng | `-font=gothiceng` | [Chart](gothiceng.png) |
| gothicger | `-font=gothicger` | [Chart](gothicger.png) |
| gothicita | `-font=gothicita` | [Chart](gothicita.png) |
| gothitt | `-font=gothitt` | [Chart](gothitt.png) |
| greekc | `-font=greekc` | [Chart](greekc.png) |
| greek | `$k...$r` or `-font=greek` | [Chart](greek.png) |
| greeks | `-font=greeks` | [Chart](greeks.png) |
| japanese | `-font=japanese` | [Chart](japanese.png) |
| markers | `-font=markers` | [Chart](markers.png) |
| mathlow | `$m...$r` or `-font=mathlow` | [Chart](mathlow.png) |
| mathupp | `-font=mathupp` | [Chart](mathupp.png) |
| meteorology | `-font=meteorology` | [Chart](meteorology.png) |
| music | `-font=music` | [Chart](music.png) |
| rowmand | `$2...$r` or `-font=rowmand` | [Chart](rowmand.png) |
| rowmans | `$1...$r` or `-font=rowmans` | [Chart](rowmans.png) |
| rowmant | `$3...$r` or `-font=rowmant` | [Chart](rowmant.png) |
| scriptc | `-font=scriptc` | [Chart](scriptc.png) |
| scripts | `-font=scripts` | [Chart](scripts.png) |
| symbolic | `$y...$r` or `-font=symbolic` | [Chart](symbolic.png) |
| timesg | `-font=timesg` | [Chart](timesg.png) |
| timesib | `-font=timesib` | [Chart](timesib.png) |
| timesi | `-font=timesi` | [Chart](timesi.png) |
| timesrb | `-font=timesrb` | [Chart](timesrb.png) |
| timesr | `-font=timesr` | [Chart](timesr.png) |
| Original Greek | `$g...$r` | [Chart](original_Greek.png) |
| Original special symbols | `$s...$e` | [Chart](original_special.png) |

## Maintaining the charts

From the repository root, run `make -C SDDSaps/doc character-charts` with Python 3,
Pillow, and DejaVu Sans / DejaVu Sans Mono installed. Regeneration updates the
PNGs, HTML gallery, and ZIP. Normal manual builds use the supplied assets and
do not require these dependencies.

The generator reads the glyph geometry from `SDDSaps/sddsplots/hershey.font`.
Its routing rules mirror `psymbol.c`, and its inline switches mirror
`graphics.c`; review these rules whenever the renderer changes. The charts
cover supported printable input characters, not every glyph stored in the font
files or characters requiring control bytes.
