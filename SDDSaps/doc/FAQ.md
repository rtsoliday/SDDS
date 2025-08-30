# SDDS FAQ

This document answers common questions about SDDS tools.
Use the table of contents below to jump to a specific topic.

## Table of Contents

### 1. General

* [1.1 Why doesn’t my `sddsxref` command add the column I requested when I use both `-take` and `-leave`?](#faq1)
* [1.2 How can I use a filename that begins with a minus sign (`-`) in an SDDS command?](#faq13)
* [1.3 How can I embed comments directly into SDDS commands when writing scripts?](#faq17)
* [1.4 How can I control the number of significant digits in a `sddscontour` topline or title?](#faq18)
* [1.5 How can I rename a column in an SDDS file?](#faq36)
* [1.6 How can I copy all parameters from one SDDS file into another?](#faq39)
* [1.7 How can I delete specific pages from an SDDS file?](#faq53)

---

### 2. File Conversion & Input

* [2.1 Why does `csv2sdds` give `invalid -columnData syntax`?](#faq10)
* [2.2 Why does `csv2sdds` produce zeros for a numeric column?](#faq11)
* [2.3 What is the best way to convert an Excel spreadsheet to SDDS format?](#faq29)
* [2.4 Why does `plaindata2sdds` misread columns after the first?](#faq27)
* [2.5 How can I convert a text table of density data into SDDS format for contour plotting?](#faq22)
* [2.6 How can I convert a tabular image file into a single-column file for contour plotting?](#faq25)
* [2.7 How can I convert color images (PNG, JPEG, etc.) into SDDS format?](#faq60)
* [2.8 Why does a wildcard `sddscombine` command fail in Tcl but work in the shell?](#faq28)

---

### 3. Plotting with `sddsplot`

* [3.1 Why do I get a "no match" error when using wildcards in column names?](#faq5)
* [3.2 How can I modify the text shown in a legend?](#faq6)
* [3.3 How can I offset the y-axis using a parameter value?](#faq8)
* [3.4 How can I add arrows with `-arrowSettings`?](#faq9)
* [3.5 How can I show a variable with an overline and subscript?](#faq12)
* [3.6 How can I strip unwanted text from legend labels?](#faq34)
* [3.7 How can I create a waterfall plot?](#faq41)
* [3.8 How can I stagger plots from multiple files so they don’t overlap?](#faq43)
* [3.9 How can I plot a horizontal line at a parameter value?](#faq55)
* [3.10 How can I display the month/day/year in a legend instead of full `TimeStamp`?](#faq62)
* [3.11 How should I structure commands with multiple `-col` and `-leg` to avoid warnings?](#faq63)
* [3.12 How can I use a parameter value to define a plot filter range?](#faq38)

---

### 4. Plotting with `sddscontour`

* [4.1 How can I print the input filename on a plot?](#faq4)
* [4.2 How can I specify a fixed maximum z-value instead of autoscaling?](#faq14)
* [4.3 Why does it fail with `Can't figure out how to turn column into 2D grid` when using `sddsspotanalysis`?](#faq21)
* [4.4 Why does it fail with `Can't figure out how to turn column into 2D grid!` when plotting output from `sddsspotanalysis`?](#faq33)
* [4.5 Why does it fail with irregularly spaced data unless I use both `-col` and `-shade`?](#faq48)
* [4.6 How can I flip or mirror an image along an axis?](#faq35)
* [4.7 How can I flip a plot about an axis to create a mirror image?](#faq47)
* [4.8 How can I restrict the vertical range displayed?](#faq58)
* [4.9 How can I generate a bare image (no borders, scales, text) with exact pixel dimensions?](#faq59)

---

### 5. Image Tools (`sddsimage*`, `tiff2sdds`)

* [5.1 How can I trim an image file to remove noisy regions?](#faq2)
* [5.2 What’s the difference between `-method=integrated` and `-method=centerline` in `sddsimageprofiles`?](#faq3)
* [5.3 Why does `sddsimageprofiles` return unexpected results?](#faq46)
* [5.4 How can I use the `Index` column from `sddsimageconvert` output as x-axis?](#faq31)
* [5.5 How can I select a specific row from an `sddsimageconvert` table?](#faq30)
* [5.6 How can I sum multiple images from `tiff2sdds`?](#faq49)

---

### 6. Data Processing (`sddsprocess`, `sddsderiv`, `sddszerofind`, Tcl scripts)

* [6.1 Why doesn’t `sddsderiv` generate the derivative column?](#faq7)
* [6.2 Why does `sddsprocess` filtering give "no rows selected"?](#faq19)
* [6.3 Why does the second column overwrite the first in `sddsprocess`?](#faq20)
* [6.4 How can I compute a running cumulative sum?](#faq52)
* [6.5 How can I select rows when the match column is numeric?](#faq40)
* [6.6 Why does `sddsprocess` return infinite values with `topLimit`?](#faq64)
* [6.7 How can I compute the average profile from a set of profiles?](#faq24)
* [6.8 How can I collect and average a column across multiple files for contour plotting?](#faq44)
* [6.9 How can I compute the average of multiple measurement columns for rows matching a parameter?](#faq61)
* [6.10 How can I convolve each page with a waveform?](#faq57)
* [6.11 How can I pass parameter values between commands?](#faq54)
* [6.12 How can I save the zero crossing from `sddszerofind` as a parameter?](#faq32)
* [6.13 How can I copy a single column value into a parameter of another file?](#faq23)
* [6.14 How can I merge selected columns from multiple SDDS files in Tcl?](#faq42)
* [6.15 How can I add symbols like `x` and `y` to parameters with `sddsprocess`?](#faq56)

---

### 7. Time & Index Handling

* [7.1 How can I create a human-readable time column?](#faq26)
* [7.2 How can I convert epoch `Time` into readable time in plots?](#faq50)
* [7.3 How can I create a continuous `TimeOfDay` across multiple days?](#faq37)

---

### 8. Specialized Fitting & Oscilloscope Data

* [8.1 Why does `sddsgenericfit` fail on sine wave fitting?](#faq15)
* [8.2 How can I plot specific pages from oscilloscope data and display the page number?](#faq16)
* [8.3 How can I convert a two-column multi-page file for `sddscontour`?](#faq45)

---

## <a id="faq1"></a>Why doesn't my `sddsxref` command add the column I requested when I use both `-take` and `-leave`?

I tried the following command:

```
sddsxref ./data/inputFile referenceFile -nowarn \
  -take=SomeColumnName -leave=* -equate=PageNumber
```

but the column from the reference file was not added to the output.

### Answer

The `-leave` option applies to the primary file and restricts which of its columns are kept. When you specify both `-take` and `-leave`, the `-leave` setting dominates. Using `-leave=*` means "keep all columns from the primary file," but it prevents the requested column from the reference file from being added.

If your goal is to add specific columns from the reference file, **omit `-leave=*`** and just use `-take`.

In general:

* Use **`-take`** when you want to bring in specific columns, parameters, or arrays from the reference file.
* Use **`-leave`** when you want to restrict which items are retained from the primary file.
* Do not use both together unless you carefully intend the interaction.

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

Use the **`-areaOfInterest=<rowStart>,<rowEnd>,<columnStart>,<columnEnd>`** option in `sddsimageprofiles` to restrict the analysis to a rectangular region of the image. Row and column indices start at **0** from the top-left corner.

For example:

```bash
sddsimageprofiles inputFile -profileType=y \
  "-columnPrefix=HLine" outputFile.y \
  -areaOfInterest=260,290,245,280

sddsplot outputFile.y -col=y,x -graph=symbol
```

This extracts the y-profile only from the specified rows and columns, reducing noise from background regions.

If you want to permanently crop the image data for later use (not just profiles), you can also run:

```bash
sddsimageconvert inputFile outputTrimmed.sdds \
  -areaOfInterest=260,290,245,280
```

and then analyze the trimmed file.

---

## <a id="faq3"></a>What is the difference between using `-method=integrated` and `-method=centerline` in `sddsimageprofiles`?

### Answer

The `-method` option controls how profiles are computed from image data:

* **`-method=integrated`** — sums pixel values across the specified dimension (rows for an x-profile, columns for a y-profile). This produces a profile proportional to total intensity but may amplify background noise, since all pixels in the strip contribute.

* **`-method=average`** — computes the mean value across the specified dimension. This keeps profiles on the same scale regardless of image size and can reduce scaling issues compared with `integrated`.

* **`-method=centerline`** — extracts data from a single central row or column of the region. This often gives a cleaner profile if the signal is strongly localized, but it may miss information from the full beam distribution.

For best results:
- Use **`integrated`** when total signal intensity matters.
- Use **`average`** when you want normalized profiles less sensitive to image size.
- Use **`centerline`** when background noise makes integration unreliable, or when the feature of interest is well-centered.

In all cases, the **`-areaOfInterest`** option can be used to restrict analysis to a smaller region, which often improves profile quality by excluding noisy background pixels.

---

## <a id="faq4"></a>How can I print the input filename on a plot when using `sddscontour`, similar to the `-filename` option in `sddsplot`?

```
sddscontour datafile.sdds -filename,edit=49d%
```

### Answer

`sddscontour` does not provide the `-filename` option that `sddsplot` supports. To display the filename, use the **`-topline`** or **`-title`** options with the filename text supplied explicitly. For example:

```bash
sddscontour datafile.sdds -topline=datafile.sdds
```

Unlike `sddsplot`, the program will not automatically insert the filename—you must pass it yourself (e.g., with a shell variable:

```bash
sddscontour "$f" -topline="$f"
```

If you want more control over placement, you can use `-string` instead, which allows positioning the text anywhere on the plot.

**Note:** If multiple files are plotted in one command, the same topline will appear for all; there is no per-file automatic filename labeling.

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

`sddsplot` supports wildcards (`*`, `?`) in column names, but they must be passed literally to the program.
If your shell (e.g., `csh` or `tcsh`) expands them as **filename patterns**, `sddsplot` never receives the wildcard and will report `"no match"`.

**Fix:** Quote or escape the column specification so the shell does not interpret it:

```bash
sddsplot -graph=line,thick=2,vary -thick=2 OUTFILE_red -file \
  "-col=Index,DEVICE:Signal.s37*p?.y" -sep -layout=2,4 \
  -leg=edit=25d \
  -dev=lpng -out=OUTFILE_red_s37_y.png
```

or equivalently:

```bash
-col=Index,DEVICE:Signal.s37*p?.y
```

Notes:

* In **bash**, quoting is usually not required but is still recommended for safety.
* In **csh/tcsh**, quoting (or escaping) is essential to prevent premature expansion.
* To check your shell, run:

```bash
echo $SHELL
```

This ensures that the wildcard expression is interpreted by `sddsplot`, not by the shell.

---

## <a id="faq6"></a>How can I modify the text shown in a legend when using `-legend` in `sddsplot`?

### Answer

When using the `-legend` option in `sddsplot`, you can control how the legend text is displayed using the **`editCommand`** field. This applies the same rules as the **`editstring`** utility, allowing you to remove, replace, or reformat parts of the original legend label.

For example, suppose the original label is:

```
S-DAQTBT:TurnsOnDiagTrig.s38p1.y
```

and you only want to keep the shorter portion:

```
s38p1.y
```

You can use:

```bash
sddsplot data.sdds -col=y \
  -legend=editCommand="%/S-DAQTBT:TurnsOnDiagTrig//"
```

This removes the matching prefix (`S-DAQTBT:TurnsOnDiagTrig`) from the legend text.

Other useful `editCommand` forms include:

* **Substitution:**
  `%/old/new/` — replaces `old` with `new`.
* **Deletion:**
  `%/unwanted//` — removes the substring `unwanted`.
* **Truncation:**
  `%49d` — keep only the first 49 characters.
* **Case changes, multiple edits, etc.** — see the `editstring` program for a full list of editing operations.

**Tip:** You can test your `editCommand` rules interactively using the `editstring` program before applying them in `sddsplot`.

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

Attempts like these will fail:

```

-offset=yParameter=-@pCentral
-offset=yParameter=-pCentral
-offset=yParameter=-"pCentral"

```

with errors such as:

```

Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsDouble)

```

### Answer

The correct syntax is:

```

-offset=yParameter=pCentral

```

This applies the parameter `pCentral` as the vertical offset.

If you need the negative of the parameter, you cannot prefix a minus sign to the name.  
Instead, add the `yInvert` keyword:

```

-offset=yParameter=pCentral,yInvert

```

Similarly, for x-axis offsets use:

```

-offset=xParameter=<parameterName>[,xInvert]

```

**Notes:**
* Do not use `@`, quotes, or leading minus signs in parameter names.  
* `xInvert` and `yInvert` provide sign reversal if needed.

---

## <a id="faq9"></a>How can I add arrows to a plot using `sddsplot` with `-arrowSettings`?

I tried adding arrows to a plot with the following command:

```
sddsplot -graph=line,thick=2 -thick=2 -filter=col,Time,1720587600.0,1720596108.0 \
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

This happens when the input text cannot be parsed as valid numeric data for the declared type. Common causes include:

* Extra header or descriptive lines before the actual data (use `-skiplines`).
* Non-numeric strings or empty fields in the column (declare a `string` column for these and later delete it).
* Wrong delimiter (set with `-separator=,` or `-separator="\t"`).
* Declaring the wrong data type (e.g., `double` vs `long`).

Typical workflow:

```bash
csv2sdds input.csv -pipe=out -skiplines=16 -separator=, \
  -col=name=Temp,type=string \
  -col=name=ColumnA,type=double | \
sddsprocess -pipe=in output.sdds -delete=column,Temp
```

Notes:

* `-skiplines=16` skips headers before the real data starts (adjust as needed).
* Temporary string columns capture text fields so they don’t corrupt numeric parsing.
* After conversion, remove unwanted string columns with `sddsprocess`.

If `csv2sdds` encounters bad numeric data, it will emit warnings and substitute zeros. Always check the program output for these messages.

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

* `$h` moves back **half a character space** (use two for one full character).
* `$a` turns on superscript, which is used to place the bar (`-`) above the character.
* `-` is the actual bar symbol drawn above the `x`.
* `$b` turns on subscript for the `0`.

The result is `x̅₀` instead of `x₀`.

---

## <a id="faq13"></a>How can I use a filename that begins with a minus sign (`-`) in an SDDS command?

When I try to pass a file whose name starts with a minus sign (`-`) to an SDDS command, the command interprets the leading `-` as an option flag. I’ve tried escaping it with `\`, surrounding it with quotes, and using `''`, but none of these work. How can I make the command accept the file without renaming it?

### Answer

Prefix a path component (e.g., `./`) so the argument doesn’t *start* with `-`:

```bash
sddsquery ./-filename.sdds
# or with an absolute/relative directory:
sddsquery /path/to/-filename.sdds
```

---

## <a id="faq14"></a>How can I specify a fixed maximum z-value in `sddscontour` instead of using autoscaling?

When comparing two contour plots side by side, the autoscaling of the z-axis can make it difficult to directly compare intensity values. For example, one plot might be scaled from -1 to 1 while another is scaled from -2 to 2, leading to mismatched color scales.

### Answer

You can override autoscaling by setting explicit shading limits with the **`-shade`** option:

```bash
sddscontour input.sdds -shade=100,-2,2
```

* `100` = number of shading levels (use fewer for coarser bins).
* `-2,2` = fixed z-range applied to all plots.

This ensures consistent color mapping across plots.

**Notes:**

* For contour lines without shading, use:

```bash
sddscontour input.sdds -scale=z,-2,2
```
* To emphasize positive/negative symmetry, choose limits like `-A,A`.

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

`-filter=column,y,-5,200` **keeps** only the rows whose `y` values lie **inside** `[-5, 200]`. If all values fall outside this interval, no rows are selected and you’ll see the warning “no rows selected for page 1.” Range endpoints are **inclusive**. To **invert** the selection (i.e., keep values *outside* the interval), add `,!` to the filter:

```bash
sddsprocess input.sdds -filter=column,y,-5,200,!
```

When the data come from `sddsimageprofiles`, be aware that `-method=integrated` **sums** profiles and can produce large magnitudes. If you intended values comparable to a single profile, use:

```bash
sddsimageprofiles image.sdds output.sdds -profileType=y -method=averaged
```

This returns the **average** profile instead of the sum, which typically matches filter ranges like `[-5, 200]` more naturally.

---

## <a id="faq20"></a>Why does the second column overwrite the first when adding multiple columns with `sddsprocess`, and how can I keep both?

A user attempted the following commands to add two new columns into a single file:

```
sddsprocess inputX.sdds output.sdds "-define=column,xCentroid,<pvnameX> 287 +"
sddsprocess inputY.sdds output.sdds "-define=column,yCentroid,<pvnameY> 194 +"
```

The result was that the `yCentroid` column overwrote the previously added `xCentroid` column in the output file.

### Answer

`sddsprocess` overwrites the specified output file each time it runs and does not preserve existing content in that file. To retain both columns, you should process the X and Y data separately, then merge the results into a single file using `sddsxref`. For example:

```
sddsprocess inputX.sdds "-define=column,xCentroid,<pvnameX> 287 +"
sddsprocess inputY.sdds "-define=column,yCentroid,<pvnameY> 194 +"
sddsxref inputX.sdds inputY.sdds output.sdds -take=yCentroid
```

This ensures that the final output contains the original contents of `inputX.sdds` along with both `xCentroid` and `yCentroid` columns.

---

## <a id="faq21"></a>Why does `sddscontour` fail with `Can't figure out how to turn column into 2D grid` when using output from `sddsspotanalysis`?

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
sddscontour output.sdds "-columnMatch=Index,rho*" -shade=10
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

sddsconvert outputTable.sdds outputTableRow.sdds \
  -retain=col,data???,Index,-4.0

```

but this removed many data columns instead of isolating the row.

### Answer

To select a row by index value, use `sddsprocess` with the `-filter` option:

```

sddsprocess outputTable.sdds outputTableRow.sdds -filter=column,Index,-4.1,-3.9

```

This keeps only rows where `Index` is near `-4.0`.

If the resulting file has columns with embedded numeric values (e.g., `data123.4`), you can convert those column names into a new numeric column (`Zpos`) using a sequence of commands:

```

sddsprocess -pipe=out outputTableRow.sdds -proc=Index,first,xoffset | \
sddsprocess -pipe -delete=col,Index | \
sddstranspose -pipe | \
sddsprocess -pipe -scan=column,Zpos,OldColumnNames,%lf,type=double,edit=4d | \
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

sddsprocess -pipe=out ./<dir>/<file>.pfit4 \
  "-define=column,dfdth,phase 3 pow 4 * Coefficient04 * phase sqr 3 * Coefficient03 * + phase 2 * Coefficient02 * + Coefficient01 +,type=double,units=MeV/c/deg" | \
sddszerofind -pipe=in ./<dir>/<file>_dfdtheta_zero \ 
  -zero=dfdth -col=phase -slopeoutput

```

This produced a file containing a single-row column with the zero crossing value.  
How can I instead store this value as a parameter back in the original file?

### Answer

Use `sddsexpand` to convert the row of column values into parameters, then transfer those parameters back to the original file with `sddsxref`. For example:

```

sddsprocess -pipe=out /tmp/input.pfit4 \
  "-define=column,dfdth,phase 3 pow 4 * Coefficient04 * phase sqr 3 * Coefficient03 * + phase 2 * Coefficient02 * Coefficient01 +,type=double,units=MeV/c/deg" | \
sddszerofind -pipe -zero=dfdth -col=phase -slopeoutput | \
sddsexpand -pipe=in /tmp/input_dfdtheta_zero

sddsxref /tmp/input.pfit4 /tmp/input_dfdtheta_zero \
  -transfer=parameter,phase,phaseSlope -nowarn

rm /tmp/input_dfdtheta_zero

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

sddscontour -shade sampleFile_plot
Error: Can't figure out how to turn column into 2D grid!

```

### Answer

Recent versions of `sddsspotanalysis` no longer produce `Variable1Name` and `Variable2Name` columns in the spot image file.  
Instead, the output contains three explicit columns: `ix`, `iy`, and `Image`, which represent the grid x-coordinate, y-coordinate, and pixel intensity.  

To use this file with `sddscontour`, specify these columns directly:

```

sddscontour sampleFile_plot -shade -xyz=ix,iy,Image

```

This tells `sddscontour` which columns form the 2D grid and which column contains the data values. Without this explicit mapping, the program cannot construct the grid and produces the error above.

---

## <a id="faq34"></a>How can I strip unwanted text from legend labels in `sddsplot`?

For example, using a command such as:

```

sddsplot -graph=line,vary,thick=2 -thick=2 
-filter=col,TimeOfDay,14.75,15.2 
inputFile.sdds 
"-col=TimeOfDay,SomeColumn*" -yscalesGroup=ID=0 
-leg ' -ylabel=TC Temperature (\$ao\$nC)' 
-col=TimeOfDay,AnotherColumn -yscalesGroup=ID=1

```

The legend may display labels like `SomeColumn*` or device-style PV names that are too long or not user-friendly. How can these be simplified?

### Answer

Use the `-legend=editCommand=<rule>` option in `sddsplot`.  
The `editCommand` applies the same syntax as the `editstring` utility, which allows pattern substitution, deletion, or truncation of legend labels.

For example:

```

sddsplot inputFile.sdds "-col=TimeOfDay,SomeColumn*" \
  -legend=editCommand="%/SomeColumn//"

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
```

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

```

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
```

This ensures the filter range uses the actual numeric value rather than the unresolved parameter name.

---

## <a id="faq39"></a>How can I copy all parameters from one SDDS file into another?

I tried the following command:

```

sddsxref inputFileWithParameters outputFile -nowarn "-transfer=param,*" "-leave=*"

```

but the parameters were not transferred, and I got warnings like:

```

warning: no parameter matches *

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
Unable to select rows--selection column is not a string (SDDS_MatchRowsOfInterest)

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

sddsplot input.table \
  -graph=sym,conn,thick=2,fill \
  -stagger=yIncrement=0.25,xIncrement=1e-7,datanames \
  "-col=Index,Signal*"

```

* `yIncrement` and `xIncrement` control the vertical and horizontal offsets between successive data sets.
* `datanames` ensures that the stagger increments are applied separately for each plotted column.

If you need separate groups of plots, combine this with `-split` as appropriate (e.g., `-split=page` or `-split=columnBin=<colname>`).

---

## <a id="faq42"></a>How can I merge selected columns from multiple SDDS files into a single file in Tcl?

I want to take a few specific columns from many files and combine them into one output file that contains only those columns merged across all input files. Running `sddscombine` inside a Tcl `foreach` loop didn’t work as expected.

For example, the failing approach looked like:

```

foreach fil $filList {
  exec sddscombine $fil output.sdds -retain=col,Xcol,Ycol,SomeData -overwrite -merge
}

```

but this overwrote the output file each time instead of merging.

### Answer

In Tcl, wildcards and lists need to be expanded before calling `sddscombine`.  
Instead of looping over files, use `glob` and `eval` to pass the full expanded list of filenames as arguments in a single command:

```

set filList [lsort [glob input*.proc]]

eval exec sddscombine $filList merged.sdds \
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

sddsplot -graph=line,vary,thick=2 -thick=2 
-factor=yMultiplier=1e3 
-stagger=xIncrement=50,yIncrement=0.0025,files 
-col=tns,SignalInv file1.proc 
-col=tns,SignalInv file2.proc 
-col=tns,SignalInv file3.proc

```

just puts the plots on top of each other.

### Answer

When using `-stagger` with the `files` keyword, all the files must be listed under a single `-col` option. For example:

```

sddsplot -graph=line,vary,thick=2 -thick=2 
-factor=yMultiplier=1e3 
-stagger=xIncrement=50,yIncrement=0.0025,files 
-col=tns,SignalInv file1.proc file2.proc file3.proc

```

This way, `sddsplot` knows to apply the stagger increments per file rather than per column.  
If you instead want the staggering applied to each column in a single file, use the `datanames` keyword:

```

sddsplot datafile.sdds -stagger=yIncrement=0.25,datanames "-col=Index,Signal*"

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
```

Explanation:

* **`sddscombine`** merges pages from multiple input files.
* **`sddsprocess -match=column,volname=<Pattern>`** filters rows by matching the `volname` column (e.g., `bxTsEMz2`).
* **`sddsprocess -process=deq,sum,deqSum -process=deq,average,deqAve`** computes the sum and average of the `deq` column.
* **`sddscollapse`** collapses multiple pages into a single table for easier plotting.
* **`sddsconvert -ascii`** creates a human-readable SDDS file.

The resulting file will contain the computed `deqSum` and `deqAve` columns, which can then be used for contour plotting or further analysis.

---

## <a id="faq45"></a>How can I convert a two-column, multi-page file into a format suitable for `sddscontour`?

I have a file with two columns (e.g., `tc` and `SigConvolve`) and multiple pages. It plots correctly with `sddsplot` using `-split=pages` and `-stagger`, but how can I make it work with `sddscontour`?

**Example command that worked with `sddsplot`:**

```bash
sddsplot -graph=line -thick=2 input.smth2 \
  -col=tc,SigConvolve -split=pages \
  -stagger=yinc=-0.04,xinc=-102.30
```

### Answer

`sddscontour` requires three quantities: an **x column**, a **y column** (or page index), and a **z column** (the data values). A simple two-column file with multiple pages cannot be used directly. To make it compatible:

1. Use `sddsprocess` to add a page index column:

```bash
sddsprocess input.smth2 -pipe=out \
  -define=column,Page,i_page,type=long | \
sddsconvert -pipe=out input_withPage.sdds
```

2. Run `sddscontour`, specifying the independent variable, page index, and data column:

```bash
sddscontour input_withPage.sdds -shade \
  -xyz=tc,Page,SigConvolve
```

This treats `tc` as the horizontal axis, `Page` as the vertical axis (replacing the staggered pages), and `SigConvolve` as the plotted intensity.

If you want a waterfall-like appearance, use the `-waterfall` option:

```bash
sddscontour input_withPage.sdds -shade \
  -waterfall=independentColumn=tc,colorColumn=SigConvolve,parameter=Page
```

This produces a stacked contour visualization similar to `sddsplot -stagger`.

---

## <a id="faq46"></a>Why does `sddsimageprofiles` return unexpected results (all points or a single point) instead of an integrated profile?

I tried to generate x- and y-profiles from a 6×11 image dataset using:

```

sddsimageprofiles -pipe=out output.sdds -profileType=x -method=integrated "-columnPrefix=data" | 
sddsprocess -pipe=in output_Intx "-define=col,aveDR,y 11 / 3.6e8 *,type=double,units=mrem/hr"

sddsimageprofiles -pipe=out output.sdds -profileType=y -method=integrated "-columnPrefix=data" | 
sddsprocess -pipe=in output_Inty "-define=col,aveDR,x 6 / 3.6e8 *,type=double,units=mrem/hr"

```

but instead of getting a true integrated profile, the first command produced all 66 points in x, and the second produced only a single point in y.

### Answer

`sddsimageprofiles` requires its input to be in a tabular "image" format with columns named:

```

Index, Data1, Data2, Data3, ...

```

If the input file does not already have this structure, the program cannot correctly interpret rows and columns, leading to unexpected results (such as returning every pixel or collapsing to a single point).  

To fix this, first run the file through `sddsimageconvert`:

```bash
sddsimageconvert output.sdds -pipe=out | \
sddsimageprofiles -pipe -profileType=x -method=integrated "-columnPrefix=data" | \
sddsprocess -pipe=in output_Intx "-define=col,aveDR,y 11 / 3.6e8 *,type=double,units=mrem/hr"
```

This ensures the data is reshaped into the expected format so `sddsimageprofiles` can compute correct x- and y-profiles.

---

## <a id="faq47"></a>How can I flip an `sddscontour` plot about an axis to create a mirror image?

For example, I want to mirror the plot horizontally or vertically without modifying the underlying data.

### Answer

`sddscontour` provides built-in options for flipping the plot:

* `-xflip` — reverses the horizontal axis.  
* `-yflip` — reverses the vertical axis.  

Example:

```bash
sddscontour input.sdds -shade -yflip
```

will produce a vertical mirror image.

If instead you want to create a new SDDS file with reversed data (rather than just flipping the display), you can preprocess the file using `sddsprocess`. For example:

```bash
sddsprocess input.sdds output.sdds \
  "-define=column,Xrev,n_rows 1 - i_row - &X ["
```

where `X` is the original x-column and `Xrev` is the reversed column.
This creates a physically mirrored dataset that can then be plotted normally.

---

## <a id="faq48"></a>Why does `sddscontour` require both `-col` and `-shade` to work when plotting irregularly spaced data?

For example, a command such as:

```

sddscontour example.sdds "-col=Index,z*" -shade

```

only produces a plot when both `-col` and `-shade` are specified.

### Answer

When plotting irregularly spaced data, `sddscontour` needs explicit instructions for both the **data columns** and the **shading method**:

* The `-col` (or `-columnMatch`) option tells `sddscontour` which columns contain the independent variables and the data values.
* The `-shade` option directs the program to create a shaded intensity map instead of a line contour. Without `-shade`, the program cannot interpolate irregular grids into a filled plot.

---

## <a id="faq49"></a>How can I sum multiple image files converted with `tiff2sdds` into a single SDDS file?

For example, given several images converted with `tiff2sdds`:

```

beforeVC_2_frame02.sdds
beforeVC_3_frame08.sdds
beforeVC_4_frame07.sdds
beforeVC_5_frame15.sdds

```

I want to add them together into a single file, such as `beforeVC.sdds`.

### Answer

Use `sddsimagecombine` to combine multiple SDDS image files into a single output. This program correctly handles the 2D array format (`Index` and `Line*` columns) produced by `tiff2sdds`.

For example:

```bash
sddsimagecombine beforeVC_2_frame02.sdds beforeVC_3_frame08.sdds \
  beforeVC_4_frame07.sdds beforeVC_5_frame15.sdds beforeVC.sdds -sum
```

The `-sum` option ensures that pixel values from each file are added together into the final image.

If you need to switch between multi-column (`Line*`) and single-column formats, you can use:

* `tiff2sdds -singleColumnMode` when creating the SDDS file, or
* `sddsimageconvert` to convert between the two formats.

---

## <a id="faq50"></a>How can I convert an epoch `Time` column into conventional human-readable time in `sddsplot`?

I have archived data with a `Time` column stored as seconds since Jan 1, 1970 (epoch time). When plotting with `sddsplot`, the x-axis shows large numbers like `1.6e9`. I’d like the axis to show days, hours, or minutes instead.

### Answer

`sddsplot` can automatically format epoch seconds into conventional time using the option:

```

-tick=xtime

```

This converts the numeric epoch values into readable labels (e.g., dates, hours, minutes) on the plot axis, adjusting dynamically with zoom level.  
To enable this, simply keep the `Time` column as epoch seconds and add `-tick=xtime` to your plotting command.

If you want a permanent human-readable column inside the SDDS file (rather than just for plotting), preprocess the file with `sddstimeconvert`. For example:

```bash
sddstimeconvert input.sdds -pipe=out \
  -breakdown=column,Time,text=TextTime | \
sddsprocess -pipe=in output.sdds \
  -reedit=column,TextTime,5d5f2D
```

This creates a new column (`TextTime`) with formatted strings such as `MM/DD` or `HH:MM`, which can then be plotted directly.

---

## <a id="faq51"></a>How can I format numeric parameter values in `sddsplot` annotations to show a fixed number of digits?

For example, using `-string=@FWHMy` places the parameter value on the plot, but I want it displayed with three significant digits. Attempts such as:

```

-string=@FWHMy,format=%.3e

```

produce errors.

### Answer

The `-string` option in `sddsplot` does not directly support a `format` specifier.  
Instead, you can use the `,edit=` feature to control how the numeric text is displayed.  
The `edit` option applies an `editstring` command, which allows truncating or reformatting the inserted value.

For example:

```

-string=@FWHMy,edit=s?/./3fze

```

This command:

* Searches for the decimal point (`.`).
* Moves three characters past it.
* Deletes all characters up to the exponent (`e`).

This effectively leaves the value in scientific notation with three decimal places before the `e`.

If you need more precise formatting control, you should format the parameter value in your script (e.g., with `printf` in Tcl or `format` in shell) before passing it to `sddsplot`. Then insert the pre-formatted string using `-string="text"`.

---

## <a id="faq52"></a>How can I compute a running cumulative sum of a column across rows in `sddsprocess`?

For example, given a file with a column `fractPass`, I want to create a new column `sumfractPass` that contains the cumulative sum of `fractPass` over rows.

### Answer

You can define a new column using an RPN expression that accumulates values across rows.  
For instance:

```bash
sddsprocess input.sdds output.sdds \
  "-define=column,sumfractPass,i_row 0 == ? i_row &fractPass [ : i_row &fractPass [ val + $ sto val"
```

Explanation:

* `i_row 0 == ? ... : ...` ensures the first row initializes properly.
* `i_row &fractPass [` retrieves the current row’s value.
* `val + $ sto val` adds the current value to the running total and stores it back into `val`.

The result is a new column `sumfractPass` that holds the cumulative sum of `fractPass` over all rows.

---

## <a id="faq53"></a>How can I delete specific pages from an SDDS file?

Suppose I have a file with 16 pages and want to remove the last page.

### Answer

Use the `-removePages` option of **sddsconvert**. For example, to delete the 16th page:

```bash
sddsconvert input.sdds output.sdds -removePages=16
```

You can also specify a range or a comma-separated list of page numbers. For example:

```bash
sddsconvert input.sdds output.sdds -removePages=1,3,5-7
```

This removes pages 1, 3, and 5 through 7, keeping the rest unchanged.

---

## <a id="faq54"></a>How can I pass parameter values (like min and max) from one SDDS command into another?

For example, suppose you compute `xMin` and `xMax` using `sddsprocess` and want to use them in another file:

```bash
sddsprocess input.xy.Table.sdds -pipe=out \
  -filter=column,Index,-0.0011,0.0011 \
  -process=Index,minimum,xMin \
  -process=Index,maximum,xMax | \
tee tempfile.sdds | \
sddsimageprofiles -pipe -columnPrefix=frequency -method=integrate -profileType=x

sddsxref outputProfiles.sdds tempfile.sdds \
  -transfer=parameter,xMin,xMax -nowarn
rm tempfile.sdds
```

Now `outputProfiles.sdds` contains the original profile data plus the `xMin` and `xMax` parameters.

### Answer

To pass parameters between SDDS programs, first compute them into a temporary file with `sddsprocess`. Then use **`sddsxref`** with `-transfer=parameter` to copy those parameter values into the target file. This avoids having to re-compute them downstream. The `tee` utility is not required—use `-pipe` consistently and rely on `sddsxref` for parameter transfer.

---

## <a id="faq55"></a>How can I plot a horizontal line at a parameter value in `sddsplot`?

For example, I want to draw a line at the value of a parameter named `Tmelt` across the plot.

A first attempt such as:

```

sddsplot input.sdds -col=z,Tmx -graph=sym,fill,scale=2 \
  -par=z,Tmelt -graph=ybar

```

does not work.

### Answer

Use the `-drawLine` option with the `y0parameter` and `y1parameter` keywords to plot a horizontal line at the parameter value. For example:

```

sddsplot input.sdds -col=z,Tmx -graph=sym,fill,scale=2 \
  -drawLine=p0value=0,p1value=1,y0parameter=Tmelt,y1parameter=Tmelt \
  "-ylabel=T\$bmax\$n (K)"

```

This draws a line across the entire x-range (`p0value=0` to `p1value=1`) at the y-value given by the parameter `Tmelt`.

---

## <a id="faq56"></a>How can I add symbols like `x` and `y` to parameters in an SDDS file using `sddsprocess`?

I want to construct a file that I can plot with `sddscontour` using parameters `Variable1Name` and `Variable2Name`. I tried:

```

sddsprocess RadDoseRate.sdds RadDoseRate.sdds.1 \
  "-define=col,data,DR 1 *,type=double,units=mrem/hr" \
  -format=par,Variable1Name,symbol=x \
  -format=par,Variable2Name,symbol=y

```

but got errors. I ended up editing the file manually with `sddsedit` to add the symbols.

### Answer

In **sddsprocess**, the `-define` option is for creating numeric parameters, while `-print` should be used to insert string values. To add symbols such as `x` or `y` to parameters, use:

```

sddsprocess input.sdds output.sdds \
  -print=par,Variable1Name,x \
  -print=par,Variable2Name,y

```

This creates parameters `Variable1Name` and `Variable2Name` as string values with the desired symbols. Unlike `-define`, which requires specifying type and units for numeric data, `-print` directly assigns string content to parameters.

---

## <a id="faq57"></a>How can I convolve each page of a multi-page SDDS file with a single impulse response waveform using `sddsconvolve`?

I want to convolve each page of a file (broken into pages using `sddsbreak`) with the same impulse response waveform. However, `sddsconvolve` does not appear to operate on separate pages in this way.

### Answer

`sddsconvolve` requires that both input files have the **same number of pages**, since it performs the convolution page by page. If one file has multiple pages (e.g., your signal data) and the impulse response has only one page, you must create a matching number of pages in the impulse response file.

You can do this with `sddscombine` by providing the same impulse file multiple times:

```bash
# Example for 10 pages
sddscombine impulse.sdds impulse.sdds impulse.sdds impulse.sdds impulse.sdds \
  impulse.sdds impulse.sdds impulse.sdds impulse.sdds impulse.sdds \
  repeatedImpulse.sdds
```

Or in a shell loop:

```bash
sddscombine $(yes impulse.sdds | head -n 10) repeatedImpulse.sdds
```

This produces `repeatedImpulse.sdds` with 10 identical pages.
Then convolve each page of the data file with the corresponding impulse page:

```bash
sddsconvolve data.sdds repeatedImpulse.sdds output.sdds
```

Each page of `data.sdds` will be convolved with the impulse waveform.

---

## <a id="faq58"></a>How can I restrict the vertical range displayed in an `sddscontour` plot?

For example, a user attempted:

```

sddscontour -shades=256 -col='Index,HLine*' -thick=2 input.sdds \
  "-Title=" "-topline=" \
  -ystrings=edit=5d,sparse=50 -scale=0,0,50,400

```

This limited the tick labels, but the data still extended above and below the region of interest.  
Using:

```

-yrange=minimum=50,maximum=400

```

changed the axis labels but did not crop the plotted data.

### Answer

`sddscontour` does not crop the underlying dataset with `-yrange`; it only changes how the axis is labeled. To actually restrict the displayed vertical region, you must **filter the data before plotting**. For example:

```bash
sddsprocess input.sdds -pipe=out -filter=column,Index,50,400 | \
sddscontour -pipe -shades=256 -col='Index,HLine*' -thick=2 \
  "-Title=" "-topline="
```

Here:

* `sddsprocess -filter=column,Index,50,400` removes rows outside the desired y-range.
* The resulting file only contains data in the range 50–400, so `sddscontour` displays just that region.

If you simply want to control axis labeling without removing data, you can continue using:

```bash
-yrange=minimum=50,maximum=400
```

but note that values outside that range will still influence the contour shading.

---

## <a id="faq59"></a>How can I generate a bare image (no borders, scales, or text) from `sddscontour` with exact pixel dimensions matching the digitizer (e.g., 640×480)?

For example, a user tried:

```bash
sddscontour -shades=gray -equalAspect -col='Index,HLine*' \
  -noborder -topline= -title= -nocolorbar -fillscreen -noscales \
  inputFile.sdds -dev=lpng -out=output.png
````

but the PNG opened in ImageJ showed **1093×842** pixels instead of the expected **640×480**.

### Answer

By default, the `-dev=png` family options in `sddscontour` use fixed device sizes (e.g., `lpng = 1093×842`, `mpng = 820×632`, etc.), not the native dimensions of the digitizer data. To obtain an image file with a **one-to-one mapping** between SDDS array dimensions and image pixels, you must select a device option that matches the digitizer.

Recent versions of SDDS include special devices for this purpose:

* **`-dev=lfgpng`** → produces 640×480 PNGs (for “LFG” frame grabber output).
* **`-dev=mv200png`** → produces 512×480 PNGs (for “MV200” digitizer output).

Example:

```bash
sddscontour inputFile.sdds -dev=lfgpng -noborder -title= -topline= -nocolorbar -noscales -out=output.png
```

This creates a bare 640×480 PNG with no labels, borders, or scales, exactly matching the digitizer resolution.

---

## <a id="faq60"></a>How can I convert color images (PNG, JPEG, etc.) into SDDS format for use with `sddscontour`?

Attempts like this often give unexpected or confusing results:

```bash
convert input.png output.tiff
tiff2sdds output.tiff output.sdds
sddscontour output.sdds "-columnmatch=Index,Line*" -shade
```

The resulting contour plot may not match what you expect, because `tiff2sdds` does not properly convert color images to grayscale intensity.

### Answer

`tiff2sdds` expects grayscale or black-and-white TIFF input. When given a color image, it sums the red, green, and blue components into an intensity value. For example:

* 100% black → 0
* 100% blue → 256
* 100% green → 256
* 100% orange → 512

This does not reflect perceived brightness, so different colors may appear with the same intensity.

**To get correct results:**

1. Save or convert the image to **grayscale (black-and-white)** before running `tiff2sdds`.
   Example with ImageMagick:

```bash
convert input.png -colorspace Gray output.tiff
tiff2sdds output.tiff output.sdds
```

2. If your data is already in an ASCII table format (e.g., BeamView `.img` with a CSV structure and header), use **`csv2sdds`** or the dedicated **`img2sdds`** tool instead of `tiff2sdds`.

This ensures the resulting SDDS file represents intensity correctly and produces valid profiles and contours.

---

## <a id="faq61"></a>How can I compute the average of multiple measurement columns for rows matching a specific parameter value?

Suppose I have an SDDS file with a column `Iset` containing values such as 40, 60, 80, 100, and 110.  
For each `Iset`, there are 12 measurement columns (e.g., one per device). I want to compute the average value of each measurement column, but only for rows where `Iset = 100`.

**Example of the original file layout:**

```

Iset    DiodeA   DiodeB   DiodeC   ...   DiodeL
40      ...
60      ...
100     ...
100     ...
...

````

### Answer

You can filter rows by the desired `Iset` value, then apply `sddsrowstats` to compute column-wise averages:

```bash
sddsprocess diodeData.sdds -pipe=out -filter=column,Iset,100,100 | \
sddsrowstats -pipe=in output.sdds "-mean=Mean,(Diode*)"
```

This does the following:

* **`sddsprocess ... -filter=column,Iset,100,100`** keeps only rows where `Iset = 100`.
* **`sddsrowstats ... -mean=Mean,(Diode*)`** computes the mean of each measurement column matching `Diode*` and outputs them as new columns prefixed with `Mean`.

If you want the averages as parameters instead of new columns, add the `-parameter` keyword:

```bash
sddsrowstats input.sdds output.sdds "-mean=Mean,(Diode*),parameter"
```

This way you do not need to transpose the data—the averages can be computed directly from the rows.

---

## <a id="faq62"></a>How can I display the month, day, and year in a legend instead of using the full `TimeStamp`?

For example:

```bash
sddsplot -graph=line,vary,thick=2 -thick=2 \
  -col=Time,Signal input1.sdds -legend=par=TimeStamp \
  -col=Time,Signal input2.sdds -legend=par=TimeStamp
```

uses the `TimeStamp` parameter, but the text is too long.
How can I shorten it to just `MM/DD/YYYY`?

### Answer

You can shorten the legend to display only the month, day, and year by using `-legend=par=TimeStamp` together with an edit command:

```bash
-legend=par=TimeStamp,editCommand=2D2FD
```

This reformats the `TimeStamp` parameter into a compact `MM/DD/YYYY` style legend entry.

---

## <a id="faq63"></a>How should I structure an `sddsplot` command with multiple `-col` and `-leg` options to avoid warnings about missing data or mismatched units?

For example, a command like:

```bash
sddsplot \
  -graph=sym,vary=type,conn=type,vary=subtype,scale=2,thickness=2 \
  -thickness=2 -yScalesGroup=nameString \
  -col=z,MaxdT -leg=spec="max temp rise" \
  -col=z,TotalE -leg=spec="dep. E" \
  -string="E"$btotal$n" =,pCoord=0.03,qCoord=1.03,scale=1.1" \
  -string=@scraperE,pCoord=0.14,qCoord=1.03,scale=1.1 \
  -string="J,pCoord=0.43,qCoord=1.03,scale=1.1" \
  -col=zseg,maxT -leg=spec="GEOM defn" \
  outputMaxT_E.sdds.totalE aaa.seg.break.maxT.collapse
```

may generate warnings such as:

```
warning: (zseg, maxT) was excluded from plot.
warning: no datanames in request found for file outputMaxT_E.sdds.totalE
Warning: not all y quantities have the same units
```

### Answer

In `sddsplot`, each **plot request** must specify its own input file **immediately after the `-col` option** that uses it. If you place all filenames at the end of the command, `sddsplot` attempts to apply every plot request to every file, which triggers warnings about missing columns or mismatched units.

The correct structure is:

```bash
sddsplot \
  -graph=sym,vary=type,conn=type,vary=subtype,scale=2,thickness=2 \
  -thickness=2 -yScalesGroup=nameString \
  -col=z,MaxdT -leg=spec="max temp rise" outputMaxT_E.sdds.totalE \
  -string="E"$btotal$n" =,pCoord=0.03,qCoord=1.03,scale=1.1" \
  -string=@scraperE,pCoord=0.14,qCoord=1.03,scale=1.1 \
  -string="J,pCoord=0.43,qCoord=1.03,scale=1.1" \
  -col=z,TotalE -leg=spec="dep. E" outputMaxT_E.sdds.totalE \
  -col=zseg,maxT -leg=spec="GEOM defn" aaa.seg.break.maxT.collapse
```

Key points:

* Place each **input file** immediately after the corresponding `-col` request.
* Use separate plot requests when legend specifications (`-leg`) differ.
* This avoids mismatched units and prevents `sddsplot` from trying to apply column requests to files that do not contain them.

---

## <a id="faq64"></a>Why does `sddsprocess` return infinite values when using `-proc=Signal,sum,...,topLimit=-0.05`?

**Example of failing command:**

```bash
sddsprocess input.break input.break.sum -nowarn \
  -proc=Signal,sum,Qint,functionOf=t,topLimit=-0.05 \
  -proc=bunchNo,first,bunchNo \
  "-define=parameter,qL,Qint 50 / 1e9 * 8.e-10 *,type=double,units=nC"
````

### Answer

The `-nowarn` option is suppressing a warning that no rows satisfy the `topLimit=-0.05` condition.
Since all `Signal` values are above –0.05, the summation is performed on an empty set of rows, which results in **`Qint` values of infinity**.

To fix this:

* Remove `-nowarn` so you can see when no data passes the filter.
* Adjust or remove the `topLimit` so that valid rows are included in the integration.
* If you intend to integrate only negative-going signals, ensure the threshold matches the actual range of your data.

In short, infinite results occur because the `topLimit` excludes all data, leaving nothing to sum.

---
