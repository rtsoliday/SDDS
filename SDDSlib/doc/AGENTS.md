# Documentation Style Guide

This directory contains Markdown files for the SDDS library.

- Use Markdown with fenced code blocks for commands and paths.
- Indent using two spaces; avoid tabs.
- Wrap lines at a reasonable length (~100 characters).
- Reference source files and directories with relative paths.


# Building
Before running `make`, ensure the following packages are installed:
`texlive-base`, `texlive-latex-base`, `texlive-latex-extra`,
`texlive-fonts-recommended`, `texlive-plain-generic`,
`texlive-extra-utils`, `tex4ht`, and `ghostscript`.

 Use the provided `Makefile` to generate PostScript, PDF and HTML output:

 ```
 make
 ```
