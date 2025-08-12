# Documentation Style Guide

This directory contains Markdown files for the SDDS library.

- Use Markdown with fenced code blocks for commands and paths.
- Indent using two spaces; avoid tabs.
- Wrap lines at a reasonable length (~100 characters).
- Reference source files and directories with relative paths.

# Building
Before running `make`, install the required LaTeX packages:

```bash
apt-get update
apt-get install -y --no-install-recommends \
  texlive-base texlive-latex-base texlive-latex-extra \
  texlive-fonts-recommended texlive-plain-generic \
  texlive-extra-utils tex4ht ghostscript
```

If an error related to `ca-certificates-java` appears, remove the OpenJDK packages:

```bash
apt-get remove --purge -y openjdk-*-jre-headless default-jre-headless default-jre
```

Use the provided `Makefile` to generate PostScript, PDF and HTML output:

```
make
```
