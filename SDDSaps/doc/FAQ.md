# SDDS FAQ

This document answers common questions about SDDS tools.
Use the table of contents below to jump to a specific topic.

## Table of Contents
1. [Why doesn't my `sddsxref` command add the column I requested when I use both `-take` and `-leave`?](#faq1)
2. [How can I trim an image file to remove noisy regions and improve profile analysis, especially for the y-profile?](#faq2)
3. [What is the difference between using `-method=integrated` and `-method=centerline` in `sddsimageprofiles`?](#faq3)
4. [How can I print the input filename on a plot when using `sddscontour`, similar to the `-filename` option in `sddsplot`?](#faq4)
5. [Why do I get a "no match" error when running `sddsplot` with wildcards in column names?](#faq5)
6. [How can I modify the text shown in a legend when using `-legend` in `sddsplot`?](#faq6)
7. [Why doesn't this `sddsderiv` command generate the derivative column for `ColumnB`?](#faq7)
8. [How can I offset the y-axis in `sddsplot` using a parameter value from an SDDS file?](#faq8)
9. [How can I add arrows to a plot using `sddsplot` with `-arrowSettings`?](#faq9)
10. [How do I fix the `invalid -columnData syntax` error when using `csv2sdds`?](#faq10)
11. [Why does `csv2sdds` produce zeros for a numeric column when converting from CSV?](#faq11)
12. [How can I show a variable with an overline and subscript in `sddsplot`?](#faq12)
13. [How can I use a filename that begins with a minus sign (`-`) in an SDDS command?](#faq13)
14. [How can I specify a fixed maximum z-value in `sddscontour` instead of using autoscaling?](#faq14)
15. [Why does `sddsgenericfit` fail when fitting oscillatory data to a sine wave?](#faq15)
16. [How can I plot specific pages from oscilloscope data and display the page number?](#faq16)
17. [How can I embed comments directly into SDDS commands when writing scripts?](#faq17)
18. [How can I control the number of significant digits when inserting a numeric value into the title or topline of an `sddscontour` plot?](#faq18)
19. [Why does `sddsprocess` with `-filter=col,y,-5,200` give the warning "no rows selected for page 1" when processing `sddsimageprofiles` data?](#faq19)
20. [Why does the second column overwrite the first when adding multiple columns with `sddsprocess`, and how can I keep both?](#faq20)
21. [Why does `sddscontour` give a `no data values in file` error when using output from `sddsspotanalysis`?](#faq21)
22. [How can I convert a text table of density data into SDDS format for contour plotting with specified z-value regions?](#faq22)
23. [How can I copy a single column value from one SDDS file into a parameter of another SDDS file?](#faq23)
24. [How can I compute the average profile from a set of profiles with shared `ypos` values?](#faq24)
25. [How can I convert a tabular image file into a single-column file for use with contour plotting?](#faq25)
26. [How can I create a human-readable time column when converting archived data to SDDS?](#faq26)
27. [Why does `plaindata2sdds` misread columns after the first when converting a plain text file?](#faq27)
28. [Why does a wildcard `sddscombine` command fail in a Tcl script but work in the shell?](#faq28)
29. [What is the best way to convert an Excel spreadsheet to SDDS format?](#faq29)
30. [How can I select a specific row from an `sddsimageconvert` table for plotting?](#faq30)
31. [How can I use the `Index` column from `sddsimageconvert` output as the x-axis in `sddsimageprofiles`?](#faq31)
32. [How can I save the zero crossing found by `sddszerofind` as a parameter in the original file?](#faq32)
33. [Why does `sddscontour` fail with `Can't figure out how to turn column into 2D grid!` when plotting output from `sddsspotanalysis`?](#faq33)
34. [How can I strip unwanted text from legend labels in `sddsplot`?](#faq34)
35. [How can I flip or mirror an `sddscontour` image along an axis?](#faq35)
36. [How can I rename a column in an SDDS file?](#faq36)
37. [How can I create a continuous `TimeOfDay` column from archived data that spans multiple days?](#faq37)
38. [How can I use a parameter value to define a plot filter range in `sddsplot`?](#faq38)
39. [How can I copy all parameters from one SDDS file into another?](#faq39)
40. [How can I select a specific row in `sddsprocess` when the match column is numeric instead of string?](#faq40)
41. [How can I create a waterfall plot using `sddsplot`?](#faq41)
42. [How can I merge selected columns from multiple SDDS files into a single file in Tcl?](#faq42)
43. [How can I stagger plots from multiple files in `sddsplot` so they don’t overlap?](#faq43)
44. [How can I collect and average a column across multiple SDDS files for use in contour plotting?](#faq44)

---

## <a id="faq1"></a>Why doesn't my `sddsxref` command add the column I requested when I use both `-take` and `-leave`?

I tried the following command:

```
sddsxref ./data/inputFile referenceFile -nowarn \
-take=SomeColumnName -leave=* -equate=PageNumber
```

but the column from the reference file was not added to the output.

### Answer

The `-leave` option overrides `-take` when both specify a given column. Using `-leave=*` means no columns are taken at all. If you only want to select specific columns, you don’t need `-leave` when you already use `-take`. Removing `-leave=*` will allow the requested column to be added. 

---

## <a id="faq2"></a>How can I trim an image file to remove noisy regions and improve profile analysis, especially for the y-profile?

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

## <a id="faq3"></a>What is the difference between using `-method=integrated` and `-method=centerline` in `sddsimageprofiles`?

### Answer

The `-method=integrated` option computes a profile by summing signal across the specified dimension, which can include contributions from background noise. In some cases this produces poor fits, especially for the y-profile.

The `-method=centerline` option instead extracts the profile along the central line of the region, often giving a cleaner result when the signal is well localized. If the integrated method produces too much noise in the background, switching to `-method=centerline` can provide a more reliable profile for fitting.

---

## <a id="faq4"></a>How can I print the input filename on a plot when using `sddscontour`, similar to the `-filename` option in `sddsplot`?

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

## <a id="faq5"></a>Why do I get a "no match" error when running `sddsplot` with wildcards in column names?

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

## <a id="faq6"></a>How can I modify the text shown in a legend when using `-legend` in `sddsplot`?

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

## <a id="faq7"></a>Why doesn't this `sddsderiv` command generate the derivative column for `ColumnB`?

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

## <a id="faq8"></a>How can I offset the y-axis in `sddsplot` using a parameter value from an SDDS file?

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

## <a id="faq9"></a>How can I add arrows to a plot using `sddsplot` with `-arrowSettings`?

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

## <a id="faq10"></a>How do I fix the `invalid -columnData syntax` error when using `csv2sdds`?

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

## <a id="faq11"></a>Why does `csv2sdds` produce zeros for a numeric column when converting from CSV?

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

## <a id="faq12"></a>How can I show a variable with an overline and subscript in `sddsplot`?

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

## <a id="faq13"></a>How can I use a filename that begins with a minus sign (`-`) in an SDDS command?

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

## <a id="faq14"></a>How can I specify a fixed maximum z-value in `sddscontour` instead of using autoscaling?

When comparing two contour plots side by side, the autoscaling of the z-axis can make it difficult to directly compare intensity values. For example, one plot might be scaled from -1 to 1 while another is scaled from -2 to 2, leading to mismatched color scales.

### Answer

You can control the vertical scale explicitly by using the `-shade` option with three arguments:

```
sddscontour input.sdds -shade=100,-2,2
```

In this example:

* `100` specifies the number of shading levels.
* `-2,2` sets the fixed z-range for the shading scale.

This ensures that both plots use the same color mapping, making visual comparison consistent.

---

## <a id="faq15"></a>Why does `sddsgenericfit` fail when fitting oscillatory data to a sine wave?

I attempted to use `sddsgenericfit` to fit oscillatory data to the function

```
y(t) = A0 + A*sin(omega*(t - t0))
```

with parameter guesses for `omega`, `t0`, `A`, and `A0`. Despite good initial estimates, the program consistently returned the error:

```
error: can't find valid initial simplex in simplexMin()
```

An example command I used was:

```
sddsgenericfit input.sine output.sineFit \
  -column=t,y -simplex=restarts=1000 \
  "-equation=t t0 - omega * sin A * A0 +" \
  -tolerance=0.1 \
  -variable=name=omega,start=0.395,lower=0.34,upper=0.44,step=0.01,heat=0.02 \
  -variable=name=t0,start=11.3,lower=10.,upper=13.,step=0.1,heat=0.2 \
  -variable=name=A,start=1,lower=0.5,upper=1.5,step=0.1,heat=0.1 \
  -variable=name=A0,start=0,lower=-0.1,upper=0.1,step=0.01,heat=0.01
```

Plotting the waveform using guess values produced a reasonable match, but the fit routine would not converge.

### Answer

At present, there is no confirmed solution for using `sddsgenericfit` in this case. However, fitting this type of data is straightforward with `sddssinefit`, which uses the model:

```
y(t) = A0 + A*sin(2*PI*<freq>*t + <phase>)
```

If your application allows, consider using `sddssinefit` instead of `sddsgenericfit` for sine-wave fitting. `sddssinefit` directly supports oscillatory functions and avoids the simplex initialization issue encountered in `sddsgenericfit`.

---

## <a id="faq16"></a>How can I plot specific pages from oscilloscope data and display the page number?

I converted oscilloscope waveform files to SDDS format using `wfm2sdds`. Each file contains multiple segments recorded in FastFrame mode. After adding a `Turn` column with `sddsprocess`, I can plot all pages with a command like:

```
sddsplot -graph=line,vary,thick=2 -thick=2 -topline=DATASET \
  -filter=col,Turn,-1,3 -split=page -separate=page \
  -col=Turn,Signal DATASET_Ch1.proc "-leg=spec=Ch1" \
  -col=Turn,Signal DATASET_Ch2.proc "-leg=spec=Ch2"
```

However:

1. How can I plot a single page at a time (e.g., page 7) for both channels so they can be compared?
2. How can the page number be displayed on the plot?

### Answer

First, define a new parameter in each processed file to store the page number:

```
sddsprocess DATASET_Ch1.proc -define=parameter,Page,i_page,type=long
sddsprocess DATASET_Ch2.proc -define=parameter,Page,i_page,type=long
```

Then use `sddsplot` with tagging and grouping options so the `Page` parameter is available:

```
sddsplot -split=page -separate=page \
  -tagRequest=@Page -groupby=tag -title=@Page \
  -thick=2 -graph=line,vary,thick=2 -topline=DATASET \
  -filter=col,Turn,-1,3 \
  -col=Turn,Signal DATASET_Ch1.proc -leg=spec=Ch1 \
  -col=Turn,Signal DATASET_Ch2.proc -leg=spec=Ch2
```

This approach:

* Displays the page number (`Page` parameter) in the plot title area.
* Groups plots by page value, allowing you to select and compare individual pages.

To generate separate image files for each page, use a template with page substitution:

```
-dev=lpng,template=DATASET.page%d.png
```

If you instead specify only `-dev=lpng -out=DATASET.png`, only the first page will be saved to a file.

---

## <a id="faq17"></a>How can I embed comments directly into SDDS commands when writing scripts?

### Answer

When writing scripts that call SDDS programs, you can embed comments directly in the command line itself. This allows you to annotate processing steps without affecting execution.

Comments are enclosed within `=` characters and can appear between options or arguments.

```
sddsprocess inputFile outputFile \
    = Find the maximum of selected columns = \
    -process=C*,max,%sMax \
    = Normalize the columns to the maxima = \
    "-redefine=col,%s,%s %sMax /,select=C*" \
    "-define=col,%s2,%s sqr,select=C*  = Take the square of the normalized data =" \
    <etc.>
```

In this example, each `=`...`=` block is treated as a comment and ignored during execution. This feature makes longer SDDS command sequences easier to read and maintain.

---

## <a id="faq18"></a>How can I control the number of significant digits when inserting a numeric value into the title or topline of an `sddscontour` plot?

When constructing a plot title or topline in `sddscontour`, simply embedding a `format=%.3e` expression in the command line will not work as expected and may cause errors such as:

```
error: only one filename accepted
```

For example, the following command fails:

```
exec sddscontour $inputFile -shade -thick=2 -logscale=.01 -scale=0,0,0,0 \
   '-topline= Max. Dose Rate (mSv/hr) = ',format=%.3e,'$Max' -swapxy \
   "-title=log\$b10\$n(dose rate (mSv/hr))" \
   "-xlabel=y(cm)" "-ylabel=z(cm)" -equalAspect &
```

### Answer

Format the value in the script before passing it to `sddscontour`. For example, in Tcl:

```
set Max [format %.3e $Max]
```

Then use the formatted variable directly:

```
-topline="Max. Dose Rate (mSv/hr) = $Max"
```

This approach ensures the number is printed with the desired number of significant digits in the plot annotation.

---

## <a id="faq19"></a>Why does `sddsprocess` with `-filter=col,y,-5,200` give the warning "no rows selected for page 1" when processing `sddsimageprofiles` data?

### Answer

The `-filter=col,y,-5,200` option only keeps rows where the values of the specified column are inside the range from `-5` to `200`. If all of the values fall outside that range, no rows will be selected, which produces the warning.

If your goal is to **keep values outside the specified range**, add `,!` to the filter option. For example:

```
sddsprocess input.sdds -filter=col,y,-5,200,!
```

This inverts the selection, keeping all rows where the column values are *not* between `-5` and `200`.

Also, note that when using `sddsimageprofiles` with `-method=integrated`, the resulting column values may be very large (e.g., in the range of 72000 to 75000). If you intended to average instead of integrate, consider using:

```
-method=average
```

to produce values on a scale consistent with your filter range.

---

## <a id="faq20"></a>Why does the second column overwrite the first when adding multiple columns with `sddsprocess`, and how can I keep both?

A user attempted the following commands to add two new columns into a single file:

```
sddsprocess inputX.sdds output.sdds "-define=column,xCentroid,<pvnameX> 287 +"
sddsprocess inputY.sdds output.sdds "-define=column,yCentroid,<pvnameY> 194 +"
```

The result was that the `yCentroid` column overwrote the previously added `xCentroid` column in the output file.

### Answer

`SDDSprocess` overwrites the specified output file each time it runs and does not preserve existing content in that file. To retain both columns, you should process the X and Y data separately, then merge the results into a single file using `sddsxref`. For example:

```
sddsprocess inputX.sdds "-define=column,xCentroid,<pvnameX> 287 +"
sddsprocess inputY.sdds "-define=column,yCentroid,<pvnameY> 194 +"
sddsxref inputX.sdds inputY.sdds output.sdds -take=yCentroid
```

This ensures that the final output contains the original contents of `inputX.sdds` along with both `xCentroid` and `yCentroid` columns.

---

## <a id="faq21"></a>Why does `sddscontour` give a `no data values in file` error when using output from `sddsspotanalysis`?

```
sddsspotanalysis vc20220427-0004 vc20220427-0004_test -background=halfwidth=0,symmetric "-imageColumn=HLine*" -spotimage=vc20220427-0004_plot

sddscontour -shade -quantity=Image vc20220427-0004_plot
Error: Can't figure out how to turn column into 2D grid!
Check existence and type of Variable1Name and Variable2Name
```

The “plot” file does not contain `Variable1Name` or `Variable2Name`, but this used to work. How can I make `sddscontour` work again?

### Answer

Instead of relying on `Variable1Name` and `Variable2Name`, explicitly specify which columns define the 2D grid and the data to plot. For example:

```
sddscontour vc20220427-0004_plot -shade -xyz=ix,iy,Image
```

Here, `ix` and `iy` are the grid coordinate columns, and `Image` is the data column. This allows `sddscontour` to construct the 2D grid properly and apply shading to the image data.

---

## <a id="faq22"></a>How can I convert a text table of density data into SDDS format for contour plotting with specified z-value regions?

```
plaindata2sdds -col=rho,type=double,units=g/cc input.txt -pipe=out -skiplines=1 -outputmode=ascii -norowcount > output.sdds
```

but I received the error:

```
Error (plaindata2sdds): invalid -column type
```

How should I convert this text file so I can produce a shaded contour plot?

### Answer

The `-col` option in `plaindata2sdds` requires you to define each column individually, including its name, type, and units. If your file has multiple columns of density values, you need to list each column separately. For example:

```
plaindata2sdds input.txt output.sdds \
  -col=rho1,double,units=g/cc \
  -col=rho2,double,units=g/cc \
  -col=rho3,double,units=g/cc \
  ... (repeat for each column) ...
  -col=rho36,double,units=g/cc \
  -outputmode=ascii
```

You can then add an index column:

```
sddsprocess output.sdds -define=column,Index,i_row,type=long
```

and create the contour plot with shading:

```
sddscontour output.sdds "-columnmatch=Index,rho*" -shade=10
```

The `-shade=10` option divides the values into 10 bins. Keep in mind that depending on your dataset, values may only fall into some of those bins, so not all ranges may appear in the contour plot.

---

## <a id="faq23"></a>How can I copy a single column value from one SDDS file into a parameter of another SDDS file?

I want to take a single value from a column in one SDDS file and add it as a parameter to a set of other SDDS files. Specifically, I have a column in `caseList.sdds` containing values that correspond to identifiers in filenames like `beamDump??.oscil` (where `??` runs from 00 to 19). For each file, I’d like to copy the column value into a parameter. I thought `sdds2stream` might help, but it wasn’t clear how to make it work.

### Answer

One way to achieve this is to first extract the desired column value into a temporary SDDS file, then transfer it into the target file as a parameter. For example, if you want to use the column named `SomeColumn` from `caseList.sdds` and add it to the corresponding `beamDump??.oscil` file based on the `Abort` index, you can use commands like:

```
sddsprocess caseList.sdds temp.sdds -match=column,Abort=18 -process=SomeColumn,last,SomeColumnCopy
sddsxref beamDump18.oscil temp.sdds -transfer=parameter,SomeColumnCopy "-leave=*" -nowarn
rm temp.sdds
```

This will create a new parameter (`SomeColumnCopy`) in `beamDump18.oscil` using the value from the appropriate row of `caseList.sdds`. You can repeat this process for each file by adjusting the `Abort` value (and corresponding filename).

---

## <a id="faq24"></a>How can I compute the average profile from a set of profiles with shared `ypos` values?

I have a set of eight SDDS files, each containing `xpos` values at the same set of `ypos` values. I would like to generate a new file that has one column with `ypos` and another column with the average value of `xpos`, so I can plot `xpos_avg` versus `ypos`. I tried:

```
sddscombine -overwrite \
slice000um.smth slice100um.smth slice200um.smth \
slice300um.smth slice400um.smth slice500um.smth \
slice600um.smth slice700um.smth \
sliceSN08_z2.smth
```

This produced a file with multiple pages. Using `-merge` concatenates the data instead of averaging. How can I create a single file with `ypos` and the average of `xpos`?

### Answer

You can use a Tcl script with `sddsprocess` and `sddsxref` to combine the data and compute the average. The script defines new columns for each `xpos` set, scales them by the number of files, and then sums them to create an average. Save the following as a script and run with `oagtclsh <filename>`:

```tcl
# Define the output file
set outputFile /tmp/sliceAverage.smth

# Set the input files
set files "slice000um.smth slice100um.smth slice200um.smth \
slice300um.smth slice400um.smth slice500um.smth \
slice600um.smth slice700um.smth"

# Count the number of input files (floating point for division)
set number [expr 1.0 * [llength $files]]

set i 1
set option ""
foreach f $files {
    # Define xpos[i] as xpos divided by the number of files
    exec sddsprocess $f -pipe=out "-define=column,xpos${i},i_row &xpos \[ $number /" \
        | sddsprocess -pipe=in /tmp/${f}.tmp -delete=column,xpos -nowarn

    if {$i == 1} {
        file copy -force /tmp/${f}.tmp $outputFile
        set option "i_row &xpos$i \[ "
    } else {
        # Merge xpos[i] into one file using shared ypos
        exec sddsxref $outputFile /tmp/${f}.tmp -equate=ypos -take=xpos$i -nowarn
        append option "i_row &xpos$i \[ + "
    }
    incr i
}

# Define new xpos as the sum of all xpos[i]
exec sddsprocess $outputFile "-define=column,xpos,$option" -nowarn
```

This produces an output file with `ypos` and the averaged `xpos` column for plotting.

---

## <a id="faq25"></a>How can I convert a tabular image file into a single-column file for use with contour plotting?

When attempting a conversion with the following command:

```
sddsimageconvert inputFile outputFile
```

the following error may appear:

```
Error (sddsimageconvert): Unknown input file type
```

This indicates that the program could not recognize the format of the input file.

### Answer

To handle tabular image data and produce a single-column output, you can use the `-multicolumn` option. This allows the program to treat the data as multiple columns, with a specified index column and prefix for naming:

```
sddsimageconvert inputFile outputFile -multicolumn=indexName=Index,prefix=HLine
```

After conversion, you may need to adjust parameters such as interval, minimum, and dimension values to properly generate a contour plot over the desired physical range.

---

## <a id="faq26"></a>How can I create a human-readable time column when converting archived data to SDDS?

```
sddsprocess -pipe=out input.proc \
  -delete=col,TimeOfDay | \
sddstimeconvert -pipe=in input.proc2 \
  -breakdown=column,Time,day=TimeOfDay
```

How can I create a human-readable time column in the SDDS file itself?

### Answer

The time data stored in the `Time` column represents the number of seconds since Jan 1, 1970. When using `sddsplot` with the `-tick=xtime` option, this is automatically converted into a human-readable time format. If "Use Improved Zoom" is enabled under *Options → Zoom*, the axis labels will adjust dynamically (e.g., showing dates, hours, or minutes depending on the zoom range).

If you need a permanent human-readable column inside the SDDS file, use `sddstimeconvert` with a text breakdown. For example:

```
sddstimeconvert input.proc -pipe=out \
  -breakdown=column,Time,text=TextTime | \
sddsprocess -pipe=in output.proc \
  -reedit=column,TextTime,5d5f2D
```

This creates a new column (e.g., `TextTime`) containing formatted strings such as `MM/DD` or other representations depending on your requirements. You can then plot or process this column alongside your data.

---

## <a id="faq27"></a>Why does `plaindata2sdds` misread columns after the first when converting a plain text file?

```
plaindata2sdds dataTable.txt dataTable.sdds \
  -inputMode=ascii "-separator= " -norowcount \
  -col=SN,long \
  -col=Nb,long \
  -col=yoff,double,units=mm \
  -col=I,double,units=mA \
  -col=Qb,double,units=nC \
  -col=emitx,double,'units=\$gm\$rm' \
  -col=emity,double,'units=\$gm\$rm' \
  -col=sigx,double,units=mm \
  -col=sigy,double,'units=A/mm\$a2\$n' \
  -col=D,double,units=MGy
```

Why is the data not being parsed correctly?

### Answer

If only the first column appears correct, the issue may be related to hidden carriage return characters in the input file. This often happens if the text file was created on a Windows system and still contains DOS-style line endings (`CRLF`).

You can convert the file to use Unix-style line endings (`LF`) before running `plaindata2sdds`. For example:

```
dos2unix dataTable.txt
```

After running this, retry the conversion command. The columns should then parse correctly in the SDDS file.

---

## <a id="faq28"></a>Why does a wildcard `sddscombine` command fail in a Tcl script but work in the shell?

```
exec sddscombine /tmp/FilePrefix_${fileTemplate}??????.sdds.proc004.sdds \
  -collapse ${fileTemplate}collapse.sdds
```

The error is:

```
Unable to open file "/tmp/FilePrefix_20250723??????.sdds.proc004.sdds" for reading (SDDS_InitializeInput)
    while executing
"exec sddscombine "/tmp/FilePrefix_${fileTemplate}??????.sdds.proc004.sdds" \
      -collapse ${fileTemplate}collapse.sdds"
```

However, if I type the equivalent command directly at the shell prompt:

```
sddscombine /tmp/FilePrefix_20250723??????.sdds.proc004.sdds \
  -collapse 20250723collapse.sdds
```

it works. Why does the script version fail, and how can I fix it?

### Answer

When using Tcl (`tclsh` or `oagtclsh`), wildcard expansion (`??????`) is not automatically performed the same way as in `bash`, `csh`, or `tcsh`. In Tcl, the string with `??????` is passed literally to the command, rather than being expanded into the list of matching files.

To make Tcl expand the filenames, you should use the `glob` command, which returns all matching files. Additionally, you need `eval` to ensure Tcl treats the expanded list as separate arguments to `sddscombine`.

An example solution is:

```
eval exec sddscombine [glob /tmp/FilePrefix_${fileTemplate}??????.sdds.proc004.sdds] \
  -collapse ${fileTemplate}collapse.sdds
```

This way:

* `glob` finds all files that match the wildcard pattern.
* `eval` ensures Tcl passes them as individual file arguments rather than one long string.

---

## <a id="faq29"></a>What is the best way to convert an Excel spreadsheet to SDDS format?

I have measurement data stored in an Excel spreadsheet. Can I use `csv2sdds` directly, or do I need to convert it to an ASCII text file first?

### Answer

SDDS tools do not read Excel `.xls` or `.xlsx` files directly.  
The recommended approach is:

1. Open the spreadsheet in Excel (or another program that can read it).
2. Save or export the data as a CSV (comma-separated values) file.
3. Use `csv2sdds` on the CSV file to create an SDDS file.

---

## <a id="faq30"></a>How can I select a specific row from an `sddsimageconvert` table for plotting?

I tried to extract the row with `Index = -4.0` using:

```

sddsconvert outputTable.sdds outputTableRow\.sdds 
-retain=col,data???,Index,-4.0

```

but this removed many data columns instead of isolating the row.

### Answer

To select a row by index value, use `sddsprocess` with the `-filter` option:

```

sddsprocess outputTable.sdds outputTableRow\.sdds -filter=column,Index,-4.1,-3.9

```

This keeps only rows where `Index` is near `-4.0`.

If the resulting file has columns with embedded numeric values (e.g., `data123.4`), you can convert those column names into a new numeric column (`Zpos`) using a sequence of commands:

```

sddsprocess -pipe=out outputTableRow\.sdds -proc=Index,first,xoffset | 
sddsprocess -pipe -delete=col,Index | 
sddstranspose -pipe | 
sddsprocess -pipe -scan=column,Zpos,OldColumnNames,%lf,type=double,edit=4d | 
sddsconvert -pipe=in finalOutput.sdds -delete=column,OldColumnNames

```

This produces a file with two useful columns: `Zpos` and the corresponding data values, suitable for plotting.

---

## <a id="faq31"></a>How can I use the `Index` column from `sddsimageconvert` output as the x-axis in `sddsimageprofiles`?

When generating profiles with `sddsimageprofiles`, I want the `Index` column from the `sddsimageconvert` output file to be used as the x-axis values in the profile file.  
For example, the file `beamDump027.sdds.gz.converted` (created by `sddsimageconvert`) should provide the `Index` column for the x-axis of the profile file `beamDump027.sdds.gz.x.sdds` (created by `sddsimageprofiles`).  
How can this be done?

### Answer

After creating the profile file, use `sddsxref` to transfer the `Index` column from the converted file into the profile file:

```

sddsxref beamDump027.x.sdds beamDump027.converted -take=Index -nowarn

```

This ensures that the `Index` values are available in the profile file, allowing you to plot **Y vs Index**.

---

## <a id="faq32"></a>How can I save the zero crossing found by `sddszerofind` as a parameter in the original file?

I used a command like:

```

sddsprocess -pipe=out ./<dir>/<file>.pfit4 
"-define=column,dfdth,phase 3 pow 4 \* Coefficient04 \* 
phase sqr 3 \* Coefficient03 \* + phase 2 \* Coefficient02 \* + 
Coefficient01 +,type=double,units=MeV/c/deg" | 
sddszerofind -pipe=in ./<dir>/<file>\_dfdtheta\_zero 
-zero=dfdth -col=phase -slopeoutput

```

This produced a file containing a single-row column with the zero crossing value.  
How can I instead store this value as a parameter back in the original file?

### Answer

Use `sddsexpand` to convert the row of column values into parameters, then transfer those parameters back to the original file with `sddsxref`. For example:

```

sddsprocess -pipe=out /tmp/input.pfit4 
"-define=column,dfdth,phase 3 pow 4 \* Coefficient04 \* 
phase sqr 3 \* Coefficient03 \* + phase 2 \* Coefficient02 \* + 
Coefficient01 +,type=double,units=MeV/c/deg" | 
sddszerofind -pipe -zero=dfdth -col=phase -slopeoutput | 
sddsexpand -pipe=in /tmp/input\_dfdtheta\_zero

sddsxref /tmp/input.pfit4 /tmp/input\_dfdtheta\_zero 
-transfer=parameter,phase,phaseSlope -nowarn

rm /tmp/input\_dfdtheta\_zero

```

This workflow:
1. Computes the zero crossing with `sddszerofind`.
2. Converts the output row into parameters with `sddsexpand`.
3. Merges the new parameters back into the original file with `sddsxref`.

---

## <a id="faq33"></a>Why does `sddscontour` fail with `Can't figure out how to turn column into 2D grid!` when plotting output from `sddsspotanalysis`?

For example, running a script that calls `sddsspotanalysis` produced output like this:

```

spotAnalysisScript -fileName sampleFile -plot 1
Results for sampleFile:
Background Level: 0.52
Integrated Spot Intensity: 9067.1
Peak Spot Intensity: 3.47
Error: Can't figure out how to turn column into 2D grid!
Check existence and type of Variable1Name and Variable2Name

```

and the follow-up `sddscontour` command failed with:

```

sddscontour -shade sampleFile\_plot
Error: Can't figure out how to turn column into 2D grid!

```

### Answer

Recent versions of `sddsspotanalysis` no longer produce `Variable1Name` and `Variable2Name` columns in the spot image file.  
Instead, the output contains three explicit columns: `ix`, `iy`, and `Image`, which represent the grid x-coordinate, y-coordinate, and pixel intensity.  

To use this file with `sddscontour`, specify these columns directly:

```

sddscontour sampleFile\_plot -shade -xyz=ix,iy,Image

```

This tells `sddscontour` which columns form the 2D grid and which column contains the data values. Without this explicit mapping, the program cannot construct the grid and produces the error above.

---

## <a id="faq34"></a>How can I strip unwanted text from legend labels in `sddsplot`?

For example, using a command such as:

```

sddsplot -graph=line,vary,thick=2 -thick=2 
-filter=col,TimeOfDay,14.75,15.2 
inputFile.sdds 
"-col=TimeOfDay,SomeColumn\*" -yscalesGroup=ID=0 
-leg ' -ylabel=TC Temperature (\$ao\$nC)' 
-col=TimeOfDay,AnotherColumn -yscalesGroup=ID=1

```

The legend may display labels like `SomeColumn*` or device-style PV names that are too long or not user-friendly. How can these be simplified?

### Answer

Use the `-legend=editcommand=<rule>` option in `sddsplot`.  
The `editcommand` applies the same syntax as the `editstring` utility, which allows pattern substitution, deletion, or truncation of legend labels.

For example:

```

sddsplot inputFile.sdds -col=TimeOfDay,SomeColumn\* 
-legend=editcommand="%/SomeColumn//"

```

This removes the text `SomeColumn` from the legend label.  
You can use other edit rules to strip prefixes, suffixes, or replace text as needed. Running the `editstring` program on the command line will show the available editing rules.

---

## <a id="faq35"></a>How can I flip or mirror an `sddscontour` image along an axis?

When plotting contour data, the image sometimes appears inverted along the x or y axis compared to the expected orientation. For example:

```

sddscontour input.sdds -shade

```

### Answer

`sddscontour` supports axis flipping with the `-xflip` and `-yflip` options.  

* Use `-xflip` to reverse the horizontal axis.  
* Use `-yflip` to reverse the vertical axis.  

For example:

```

sddscontour input.sdds -shade -yflip

```

will mirror the image vertically.  

If you need to rotate the image instead, use the `-transpose` option (once for 90° rotation, twice for 180°, etc.). Combining transpose with `-xflip` or `-yflip` provides full control over the orientation.

---

## <a id="faq36"></a>How can I rename a column in an SDDS file?

I want to change the name of an existing column in an SDDS file without altering the data.  
For example, I need to rename `OldColumn` to `NewColumn`.

### Answer

Use the `-rename=column` option of `sddsconvert`. For example:

```

sddsconvert input.sdds output.sdds -rename=column,OldColumn=NewColumn

```

This creates a copy of `input.sdds` with the column renamed.  
If you omit the output file, `sddsconvert` will overwrite the input file:

```

sddsconvert input.sdds -rename=column,OldColumn=NewColumn

```

Both forms are valid. The explicit output form is safer if you want to keep the original file unchanged.

---

## <a id="faq37"></a>How can I create a continuous `TimeOfDay` column from archived data that spans multiple days?

For example, if `TimeOfDay` resets to 0 after midnight, how can I generate a column that keeps increasing past 24 hours?

### Answer

Compute a continuous hour counter from the epoch `Time` by breaking it into **Julian day** and **hour-of-day**, then combine them:

```bash
sddstimeconvert input.sdds -pipe=out \
  -breakdown=column,Time,julianDay=JDay,hour=HourOfDay | \
sddsprocess -pipe=in output.sdds \
  "-define=column,TimeOfDayCont,JDay 0 &JDay [ - 24 * HourOfDay +,type=double,units=h"
````

Explanation:

* `sddstimeconvert ... -breakdown=...,julianDay=JDay,hour=HourOfDay` produces `JDay` (day index) and `HourOfDay` (0–24).
* In `sddsprocess`, `0 &JDay [` grabs the **first** `JDay` value; the RPN computes
  `TimeOfDayCont = (JDay - JDay[0]) * 24 + HourOfDay`, which increases past 24 whenever the day rolls over.


---

## <a id="faq38"></a>How can I use a parameter value to define a plot filter range in `sddsplot`?

For example, after computing the minimum of a column and saving its index as a parameter (`minIndex`), I tried:

```

sddsplot data.proc -col=Index,ColumnX 
-graph=line,vary,thick=2 
-filter=col,Index,1999,minIndex

````

but it did not work.

### Answer

The `-filter` option in `sddsplot` does not accept parameter names directly.  
Instead, you must first extract the parameter value using `sdds2stream` and store it in a shell or Tcl variable. Then pass the numeric value into the filter expression.

For example in Tcl:

```tcl
set minIndex [exec sdds2stream data.proc -para=minIndex]

exec sddsplot data.proc -col=Index,ColumnX \
  -graph=line,vary,thick=2 \
  -filter=col,Index,1999,$minIndex
````

This ensures the filter range uses the actual numeric value rather than the unresolved parameter name.

---

## <a id="faq39"></a>How can I copy all parameters from one SDDS file into another?

I tried the following command:

```

sddsxref inputFileWithParameters outputFile -nowarn "-transfer=param,*" "-leave=*"

```

but the parameters were not transferred, and I got warnings like:

```

warning: no parameter matches \*

```

### Answer

In `sddsxref`, the **first file listed is the destination** (the file that will receive new data), and the **second file is the source** (the file from which data is copied).  
If you list them in the wrong order, no parameters will be transferred.

For example, to copy all parameters from `sourceFile.sdds` into `targetFile.sdds`, use:

```

sddsxref targetFile.sdds sourceFile.sdds -nowarn "-transfer=parameter,*" "-leave=*"

```

This ensures that every parameter from the source file is added to the target file while leaving existing content unchanged.

---

## <a id="faq40"></a>How can I select a specific row in `sddsprocess` when the match column is numeric instead of string?

When I try to select rows based on a numeric column, I get an error such as:

```

Error for sddsprocess:
Unable to select rows--selection column is not a string (SDDS\_MatchRowsOfInterest)

```

### Answer

The `-match` option in `sddsprocess` only works with string-type columns.  
For numeric columns, you must instead use the `-filter` option with a numeric range.

For example, if you want to select the row where `nreg = 2`, use a small range around that value:

```

sddsprocess input.sdds output.sdds -filter=column,nreg,1.99,2.01

```

This will keep only the rows where `nreg` is approximately equal to 2.  
Adjust the tolerance (`1.99–2.01`) as needed for your data.

---

## <a id="faq41"></a>How can I create a waterfall plot using `sddsplot`?

I want to plot multiple waveforms in a "waterfall" style, offset so they don’t overlap. I tried a command like:

```

sddsplot 
-graph=sym,vary=type,vary=subtype,conn=subtype,scale=1.5,conn,thick=2,fill 
-stagger=yIncrement=0.25,xIncrement=1e-7 
-leg -thick=2 -filter=col,Index,35,100 input.table 
-col=Index,Signal1.2374e-06 
-col=Index,Signal1.2382e-06 
-col=Index,Signal1.239e-06 
...
-col=Index,Signal1.2454e-06

```

but the staggering didn’t apply as expected.

### Answer

Use the `-stagger` option with the `datanames` keyword so that the stagger increments apply to each column being plotted. For example:

```

sddsplot input.table 
-graph=sym,conn,thick=2,fill 
-stagger=yIncrement=0.25,xIncrement=1e-7,datanames 
-col=Index,Signal\*

```

* `yIncrement` and `xIncrement` control the vertical and horizontal offsets between successive data sets.
* `datanames` ensures that the stagger increments are applied separately for each plotted column.

If you need separate groups of plots, combine this with `-split` as appropriate (e.g., `-split=page` or `-split=columnBin=<colname>`).

---

## <a id="faq42"></a>How can I merge selected columns from multiple SDDS files into a single file in Tcl?

I want to take a few specific columns from many files and combine them into one output file that contains only those columns merged across all input files. Running `sddscombine` inside a Tcl `foreach` loop didn’t work as expected.

For example, the failing approach looked like:

```

foreach fil \$filList {
exec sddscombine \$fil output.sdds -retain=col,Xcol,Ycol,SomeData&#x20;
-overwrite -merge
}

```

but this overwrote the output file each time instead of merging.

### Answer

In Tcl, wildcards and lists need to be expanded before calling `sddscombine`.  
Instead of looping over files, use `glob` and `eval` to pass the full expanded list of filenames as arguments in a single command:

```

set filList \[lsort \[glob input\*.proc]]

eval exec sddscombine \$filList merged.sdds&#x20;
-retain=col,Xcol,Ycol,SomeData -overwrite -merge

```

* `glob` finds all files that match the pattern.
* `lsort` ensures consistent ordering.
* `eval` expands the list into individual arguments so `sddscombine` sees them as multiple files.

This way, all files are combined at once, keeping only the specified columns.

---

## <a id="faq43"></a>How can I stagger plots from multiple files in `sddsplot` so they don’t overlap?

I want to plot several files together and offset them like a mountain range. Using commands such as:

```

sddsplot -graph=line,vary,thick=2 -thick=2&#x20;
-factor=yMultiplier=1e3&#x20;
-stagger=xIncrement=50,yIncrement=0.0025,files&#x20;
-col=tns,SignalInv file1.proc&#x20;
-col=tns,SignalInv file2.proc&#x20;
-col=tns,SignalInv file3.proc

```

just puts the plots on top of each other.

### Answer

When using `-stagger` with the `files` keyword, all the files must be listed under a single `-col` option. For example:

```

sddsplot -graph=line,vary,thick=2 -thick=2&#x20;
-factor=yMultiplier=1e3&#x20;
-stagger=xIncrement=50,yIncrement=0.0025,files&#x20;
-col=tns,SignalInv file1.proc file2.proc file3.proc

```

This way, `sddsplot` knows to apply the stagger increments per file rather than per column.  
If you instead want the staggering applied to each column in a single file, use the `datanames` keyword:

```

sddsplot datafile.sdds -stagger=yIncrement=0.25,datanames -col=Index,Signal\*

```

---

## <a id="faq44"></a>How can I collect and average a column across multiple SDDS files for use in contour plotting?

I have a set of files named `MTUPLE.sdds` in different directories.  
I want to extract values from the column `deq` only where `volname` matches a given string, then compute the sum and average across all files, and save the result into a single SDDS file for later use with `sddscontour`.

### Answer

You can use a pipeline of SDDS tools:

```bash
sddscombine file1.sdds file2.sdds ... -pipe=out | \
  sddsprocess -pipe -match=column,volname=<Pattern> | \
  sddsprocess -pipe -process=deq,sum,deqSum -process=deq,average,deqAve | \
  sddscollapse -pipe | \
  sddsconvert -pipe /tmp/output.sdds -ascii
````

Explanation:

* **`sddscombine`** merges pages from multiple input files.
* **`sddsprocess -match=column,volname=<Pattern>`** filters rows by matching the `volname` column (e.g., `bxTsEMz2`).
* **`sddsprocess -process=deq,sum,deqSum -process=deq,average,deqAve`** computes the sum and average of the `deq` column.
* **`sddscollapse`** collapses multiple pages into a single table for easier plotting.
* **`sddsconvert -ascii`** creates a human-readable SDDS file.

The resulting file will contain the computed `deqSum` and `deqAve` columns, which can then be used for contour plotting or further analysis.

---
