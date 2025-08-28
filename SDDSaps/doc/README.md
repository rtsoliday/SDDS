# SDDS FAQ

### Question

Why doesn’t my `sddsxref` command add the column I requested when I use both `-take` and `-leave`?

I tried the following command:

```
sddsxref ./data/inputFile referenceFile -nowarn \
-take=SomeColumnName -leave=* -equate=PageNumber
```

but the column from the reference file was not added to the output.

### Answer

The `-leave` option overrides `-take` when both specify a given column. Using `-leave=*` means no columns are taken at all. If you only want to select specific columns, you don’t need `-leave` when you already use `-take`. Removing `-leave=*` will allow the requested column to be added.&#x20;

---

### Question

How can I trim an image file to remove noisy regions and improve profile analysis, especially for the y-profile?

For example, given an image file with systematic noise in the background, the following commands produced a good x-profile but a poor y-profile fit:

```bash
sddsimageprofiles -pipe=out $inputFile -profileType=x \
  -method=integrated "-columnPrefix=HLine" | \
  sddsprocess -pipe=in ${inputFile}.Intx

sddsimageprofiles -pipe=out $inputFile -profileType=y \
  -method=integrated "-columnPrefix=HLine" | \
  sddsprocess -pipe=in ${inputFile}.Inty
```

A trimming attempt was made using `sddsconvert`:

```bash
sddsconvert inputFile outputFile_filt \
  "-delete=col,HLine0*,HLine3*" \
  "-delete=col,HLine1*,HLine4*"
```

But it was unclear if this was the best way to restrict the analysis region.

### Answer

To reduce noise from unwanted regions, use the `-areaOfInterest=<rowStart>,<rowEnd>,<columnStart>,<columnEnd>` option with `sddsimageprofiles`. This allows you to define a rectangular region within the image for profile extraction.

For example:

```bash
sddsimageprofiles inputFile -profileType=y \
  "-columnPrefix=HLine" outputFile.y \
  -areaOfInterest=260,290,245,280

sddsplot outputFile.y -col=y,x -graph=symbol
```

This trims the analysis to only the specified rows and columns, avoiding background noise outside the signal region.

---

### Question

What is the difference between using `-method=integrated` and `-method=centerline` in `sddsimageprofiles`?

### Answer

The `-method=integrated` option computes a profile by summing signal across the specified dimension, which can include contributions from background noise. In some cases this produces poor fits, especially for the y-profile.

The `-method=centerline` option instead extracts the profile along the central line of the region, often giving a cleaner result when the signal is well localized. If the integrated method produces too much noise in the background, switching to `-method=centerline` can provide a more reliable profile for fitting.

---

### Question

How can I print the input filename on a plot when using `sddscontour`, similar to the `-filename` option in `sddsplot`? For example:

```
sddscontour datafile.sdds -filename,edit=49d%
```

### Answer
`sddscontour` does not support the `-filename` option used in `sddsplot`. However, you can place text (such as the filename) on the plot by using the `-topline` option. For example:

```
sddscontour datafile.sdds -topline=datafile.sdds
```

This will draw the specified text string at the top of the plot. If you want the actual filename to appear automatically, you will need to supply it explicitly to `-topline`.

---

### Question

Why do I get a "no match" error when running `sddsplot` with wildcards in column names?

**Example of failing command:**

```
sddsplot -graph=line,thick=2,vary -thick=2 OUTFILE_red -file \
-col=Index,DEVICE:Signal.s37*p?.y -sep -layout=2,4 \
-leg=edit=25d \
-dev=lpng -out=OUTFILE_red_s37_y.png
```

### Answer

This error can occur when using a shell that expands wildcard characters (`*` or `?`) before they are passed to `sddsplot`. In `csh` or `tcsh`, the shell interprets these characters as filename patterns, which prevents `sddsplot` from receiving them properly.

To avoid this, enclose the column argument in quotes:

```
sddsplot -graph=line,thick=2,vary -thick=2 OUTFILE_red -file \
"-col=Index,DEVICE:Signal.s37*p?.y" -sep -layout=2,4 \
-leg=edit=25d \
-dev=lpng -out=OUTFILE_red_s37_y.png
```

In `bash`, quoting is not necessary, but in `csh`/`tcsh` it is required.
To check which shell you are using, run:

```
echo $SHELL
```

---

### Question

How can I modify the text shown in a legend when using `-legend` in `sddsplot`?

### Answer

When using the `-legend` option in `sddsplot`, the legend label can be modified with the `editcommand` field. The `editcommand` applies string-editing instructions that allow removal or replacement of portions of the original label. This is useful when the original data name is long or contains prefixes that are not needed in the display.

For example, if the column or parameter name is:

```
S-DAQTBT:TurnsOnDiagTrig.s38*p?.y
```

and you only want the portion:

```
s38*p?.y
```

to appear in the legend, you can apply an editing command.

The editing commands are those accepted by the `editstring` program, which is also available separately. Running `editstring` from the command line will print a usage message describing the available operations. These same editing commands are used inside `sddsplot` when supplied to the `editcommand` field.

A simplified example command might look like:

```
sddsplot datafile.sdds -column=y -legend=editcommand="%/S-DAQTBT:TurnsOnDiagTrig//"
```

The actual substitution pattern depends on what portion of the string you want to keep or remove. Text substitutions (`%/old/new/`) and other editing operations are supported as described in the `editstring` documentation.

---
































