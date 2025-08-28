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

### Answer

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

### Question

How do I fix the `invalid -columnData syntax` error when using `csv2sdds`?

A user attempted to convert a CSV file to SDDS format with the following command:

```
csv2sdds IrradiationDataLegacyTrim.csv IrradiationDataLegacyTrim.sdds \
-skiplines=1 \
-col=name=ID,type=char \
-col=name=Vref,type=float,units=V \
-col=name=V240305,type=float,units=V \
-col=name=V240307,type=float,units=V \
-col=name=V240327,type=float,units=V \
-col=name=V240521,type=float,units=V
```

This produced the error:

```
Error (csv2sdds): invalid -columnData syntax
```

### Answer

The `type` field in the `-col` option must use the full keyword `character` rather than the shorthand `char`. The program does not recognize `char` as a valid type.

Replace `type=char` with `type=character`. For example:

```
-col=name=ID,type=character
```

This should allow the command to run without triggering the syntax error.

---

### Question

Why does `csv2sdds` produce zeros for a numeric column when converting from CSV?

A user attempted to convert a CSV file into SDDS format using:

```
csv2sdds input.csv output.sdds -col=name=ColumnA,type=double
```

However, all values for `ColumnA` in the SDDS file appeared as zeros.

### Answer

This issue can occur if the CSV file has formatting that prevents correct parsing. Common problems include:

* Extra header or descriptive lines before the data.
* Columns containing empty strings or non-numeric text that do not match the declared type.
* Misaligned data due to delimiters or unexpected characters.

One approach is to skip the non-data lines, define the columns explicitly, and remove any temporary placeholder columns. For example:

```
csv2sdds input.csv -pipe=out -skiplines=16 \
  -col=name=Temp,type=string \
  -col=name=ColumnA,type=double | \
sddsprocess -pipe=in /tmp/junk -delete=column,Temp
```

Here:

* `-skiplines=16` ignores header or formatting lines before the actual data.
* `-col=name=Temp,type=string` captures a non-numeric field so it does not corrupt numeric parsing.
* `sddsprocess ... -delete=column,Temp` removes the temporary column after conversion.

Adjust the number of skipped lines and column definitions as needed based on the structure of your CSV file.

---

### Question

How can I place a bar over a character in *sddsplot*? For example, I tried to show a variable `x` with a subscript (`x₀`), but instead I want `x̅₀` (x with a bar on top and a subscript 0). My command looked something like this:

```bash
sddsplot -thick=2 -graph=sym,conn,fill,thick=2  input_file.sdds \
-col=ColumnX,ColumnY \
'-string=x$b0$n = ,pCoord=0.05,qCoord=0.93,scale=1.1' \
-string=@Value1,format=%.3f,pCoord=0.15,qCoord=0.93,scale=1.1 \
"-string=units,pCoord=0.29,qCoord=0.93,scale=1.1" \
'-string=$gs$r$bx$n = ,pCoord=0.05,qCoord=0.83,scale=1.1' \
-string=@Value2,format=%.3f,pCoord=0.15,qCoord=0.83,scale=1.1 \
"-string=units,pCoord=0.26,qCoord=0.83,scale=1.1"
```

This produced `x₀`, but I want to display `x̅₀`.

### Answer

You can use the following syntax to place a bar over `x` while keeping the subscript `0`:

```bash
-string=x$h$h$a-$b0
```

Explanation of escape sequences:

* `$h` moves one character back (two are required to align the bar correctly).
* `$a` turns on superscript, which is used to place the bar (`-`) above the character.
* `-` is the actual bar symbol drawn above the `x`.
* `$b` turns on subscript for the `0`.

The result is `x̅₀` instead of `x₀`.

---

### Question

How can I use a filename that begins with a minus sign (`-`) in an SDDS command?

When I try to pass a file whose name starts with a minus sign (`-`) to an SDDS command, the command interprets the leading `-` as an option flag. I’ve tried escaping it with `\`, surrounding it with quotes, and using `''`, but none of these work. How can I make the command accept the file without renaming it?

### Answer

In many UNIX and Tcl commands, a `--` argument can be used to indicate the end of options. Everything following it will then be treated as a filename, even if it starts with `-`.

For SDDS commands that accept standard input, one possible workaround without renaming the file is to use a pipeline, for example:

```
cat -- -filename.sdds | sddsquery -pipe=in
```

This forces `cat` to read the file named `-filename.sdds` and then pipes the data into the SDDS command.

If the command you are using does not support `-pipe`, or if piping isn’t appropriate, the most reliable solution remains renaming the file to remove the leading dash.

---














