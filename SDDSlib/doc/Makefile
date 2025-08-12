
all:: SDDS.ps SDDS.pdf html

SDDS.ps:
	rm -f SDDS.aux SDDS.dvi SDDS.log SDDS.toc
	latex SDDS
	latex SDDS
	dvips SDDS -o SDDS.ps
	rm -f SDDS.toc SDDS.out SDDS.aux SDDS.log SDDS.dvi 

SDDS.pdf: SDDS.ps
	ps2pdf13 SDDS.ps SDDS.pdf

html:
	mkdir -p SDDS
	htlatex SDDS "html,3" "" "-dSDDS/"
	rm -f *.html *.png *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

clean::
	rm -f SDDS.aux SDDS.dvi SDDS.log SDDS.toc SDDS.ps SDDS.pdf
	rm -rf SDDS
