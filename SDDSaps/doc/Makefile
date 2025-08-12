
all:: SDDStoolkit.ps SDDStoolkit.pdf html

SDDStoolkit.ps:
	rm -f SDDStoolkit.aux SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.out
	latex SDDStoolkit
	latex SDDStoolkit
	dvips SDDStoolkit -o SDDStoolkit.ps
	rm -f SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.aux SDDStoolkit.out

SDDStoolkit.pdf: SDDStoolkit.ps
	ps2pdf13 SDDStoolkit.ps SDDStoolkit.pdf

html:
	mkdir -p SDDStoolkit
	htlatex SDDStoolkit "html,3" "" "-dSDDStoolkit/"
	rm -f *.html *.png *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

clean::
	rm -f SDDStoolkit.aux SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.ps SDDStoolkit.pdf
	rm -rf SDDStoolkit
