# Documentation Style Guide

This directory contains LaTeX source files describing each SDDS command line tool. Follow these conventions when adding or updating documentation.

## File naming
- One program per `.tex` file.
- File names use the command name, e.g. `sddsplot.tex`.

## Structure
Each program is documented using the `sddsprog` environment defined in `sddstoolkit.sty`:

```
\begin{sddsprog}{<toolName>}
\item \textbf{description:} one or more paragraphs describing the program.
\item \textbf{examples:}
\begin{verbatim}
command examples...
\end{verbatim}
\item \textbf{synopsis:}
\begin{verbatim}
usage: <toolName> [options]
\end{verbatim}
\item \textbf{switches:} optional `itemize` block describing command-line switches.
\item \textbf{files:} optional block describing input or output files.
\item \textbf{see also:} optional references using `\progref{otherTool}`.
\item \textbf{author:} Name, ANL/APS.
\end{sddsprog}
```

Use the `verbatim` environment for both examples and synopsis sections to ensure consistent formatting.

## Formatting
- Indent using two spaces; do not use tabs.
- Use `\verb|command|` or `{\tt text}` for inline code.
- Keep LaTeX commands and environments consistent with the existing files.

## Building
Before running `make`, install the required LaTeX packages:

```bash
apt-get update
apt-get install -y --no-install-recommends \
  texlive-base texlive-latex-base texlive-latex-extra \
  texlive-fonts-recommended texlive-plain-generic \
  texlive-extra-utils tex4ht ghostscript latex2html \
  texlive-font-utils
```

If an error related to `ca-certificates-java` appears, remove the OpenJDK packages:

```bash
apt-get remove --purge -y openjdk-*-jre-headless default-jre-headless default-jre
```

Use the provided `Makefile` to generate PostScript, PDF and HTML output:

```
make
```

## Updating the FAQ

- The FAQ lives in `README.md`.
- Add new entries at the end of the file.
- Each entry uses the existing structure:
  - `### Question` followed by the question text.
  - `### Answer` followed by the answer text.
  - A line containing only `---` separates entries.
- Preserve this format for any new questions and answers.

