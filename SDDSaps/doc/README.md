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

### Question
I tried running the following command with `sddsderiv`:

```
sddsprocess -pipe=out inputFile.sdds \
-filter=col,relativeTime,0.5683,0.5688 | \
sddsderiv -pipe \
-differentiate=ColumnA,ColumnB -versus=relativeTime | \
sddsprocess -pipe=in outputFile.sdds \
"-define=col,dColAdt,ColumnADeriv -1 *,type=double,units=Counts/s" \
"-define=col,dColBdt,ColumnBDeriv -1 *,type=double,units=Counts/s" \
-proc=dColAdt,fwhm,FWHM_A,functionOf=relativeTime \
-proc=dColBdt,fwhm,FWHM_B,functionOf=relativeTime
```

But I received errors such as:

```
unknown token: ColumnBDeriv
too few items on stack (multiply)
*stop*
Error for sddsprocess:
Unable to compute rpn expression--rpn error (SDDS_ComputeDefinedColumn)
```

Why isn’t the derivative column being generated for `ColumnB`?

### Answer

The problem is with the way multiple columns were specified in the `-differentiate` option. In **sddsderiv**, if you list a second column after the first, that second entry is treated as an *optional sigma column*, not as an additional column to differentiate.

So the following is incorrect:

```
-differentiate=ColumnA,ColumnB
```

Instead, each column you want differentiated must be specified with its own `-differentiate` option, for example:

```
-differentiate=ColumnA -differentiate=ColumnB
```

This ensures that derivative columns will be properly generated for both `ColumnA` and `ColumnB`.

---

### Question

How can I use a parameter value from an SDDS file to offset the y-axis in `sddsplot`? When I tried commands like the following, I received errors stating the parameter name was unrecognized:

```
sddsplot -graph=dot -thick=2 track-5nC.w2 \
  -split=page -separate=page \
  -col=dt,p \
  -offset=yparameter=-@pCentral

sddsplot -graph=dot -thick=2 track-5nC.w2 \
  -split=page -separate=page \
  -col=dt,p \
  -offset=yparameter=-pCentral

sddsplot -graph=dot -thick=2 track-5nC.w2 \
  -split=page -separate=page \
  -col=dt,p \
  -offset=yparameter=-"pCentral"
```

Each attempt resulted in:

```
Error:
Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsDouble)
```

## Answer

When using `-offset` with a parameter, the syntax does not accept embedded signs or quotes with the parameter name. Instead, specify the parameter name directly, and if needed, combine it with the `yinvert` option to flip the sign. For example:

```
-offset=yparameter=pCentral,yinvert
```

This approach applies the parameter value `pCentral` as the y-offset while inverting the direction if required.

---

### Question

How can I add arrows to a plot using `sddsplot` with `-arrowSettings`?

I tried adding arrows to a plot with the following command:

```
sddsplot -grap=line,thick=2 -thick=2 -filter=col,Time,1720587600.0,1720596108.0 \
-col=Time,S-DCCT:CurrentM datafile.sdds -tick=xtime \
-arrowSettings=scale=1.0,barblength=0.1,barbAngle=0,linetype=1,centered,cartesian,0.1,0.1,singleBarb,thick=1
```

but received the error:

```
unknown keyword/value given: 0.1
error: invalid -arrowsettings syntax
```

What is the correct way to use `-arrowSettings` in this situation?

### Answer

The error occurs because the option string contains extra numeric values (`0.1,0.1`) that are not valid keywords. A corrected version of the option would look like:

```
-arrowSettings=scale=1.0,barblength=0.1,barbAngle=0,linetype=1,centered,cartesian,singleBarb,thick=1
```

In addition, when using the `cartesian` mode for arrows, `sddsplot` requires a second set of x and y columns to specify the vector components. This means the `-col` option must be modified to include two pairs of x and y values. For example:

```
-col=Time,(S-DCCT:CurrentM),Time,(S-DCCT:CurrentM)
```

The first pair provides the data for plotting, while the second pair provides the vector data for the arrows. Depending on the intended analysis, the second pair may reference different columns than the first.

If the `-arrowSettings` option is omitted entirely, no data may be plotted because filtering options (such as `-filter`) can remove all points. Always verify that the chosen filter and column specifications produce valid data for both the line and arrow overlays.

---














