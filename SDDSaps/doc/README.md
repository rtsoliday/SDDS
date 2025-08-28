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

### Question

How can I specify a fixed maximum z-value in `sddscontour` instead of using autoscaling?

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

### Question

Why does `sddsgenericfit` fail when fitting oscillatory data to a sine wave?

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

### Question

How can I plot specific pages from oscilloscope data and display the page number?

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

### Question

How can I embed comments directly into SDDS commands when writing scripts?

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

### Question

How can I control the number of significant digits when inserting a numeric value into the title or topline of an `sddscontour` plot?

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

### Question

Why does `sddsprocess` with `-filter=col,y,-5,200` give the warning *“no rows selected for page 1”* when processing data from `sddsimageprofiles`?

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

### Question

When I try to add two new data columns to an existing SDDS file using `sddsprocess`, the second column overwrites the first. Why does this happen, and how can I correctly combine both columns into the same file?

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

### Question

When I collect frame grabber images and analyze them with `sddsspotanalysis`, I generate a “plot” file. However, when I try to use `sddscontour` on this file, I get the following error:

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

### Question

I have a text file with many rows and columns of density data. I want to convert it into SDDS format so that I can make a contour plot. The values range from 0 up to about 2.7, and I’d like to divide the data into 10 density regions (e.g., 0–0.27, 0.27–0.54, …, 2.43–2.7). I tried the following command:

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

### Question

How can I copy a single column value from one SDDS file into a parameter of another SDDS file?

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

### Question

How can I compute the average profile from a set of profiles with shared `ypos` values?

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

### Question

How can I convert a tabular image file into a single-column file for use with contour plotting?

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
