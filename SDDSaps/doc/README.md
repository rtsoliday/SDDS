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

Here’s the email thread rewritten into a Markdown FAQ in the style of the SDDS `README.md` documentation. I kept it to a single question/answer, obscured private details, and included the failing example.

---

### How can I print the input filename on a plot when using `sddscontour`, similar to the `-filename` option in `sddsplot`?

**Question:**
When running `sddscontour`, is there a way to display the name of the input file directly on the generated plot, as can be done in `sddsplot`? For example:

```
sddscontour datafile.sdds -filename,edit=49d%
```

**Answer:**
`sddscontour` does not support the `-filename` option used in `sddsplot`. However, you can place text (such as the filename) on the plot by using the `-topline` option. For example:

```
sddscontour datafile.sdds -topline=datafile.sdds
```

This will draw the specified text string at the top of the plot. If you want the actual filename to appear automatically, you will need to supply it explicitly to `-topline`.

---

