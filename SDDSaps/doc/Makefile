
.PHONY: html character-charts install-character-charts

PYTHON ?= python3
CHART_DIR = sddsplot-character-charts

all:: SDDStoolkit.ps SDDStoolkit.pdf html

# Optional regeneration; normal builds use the supplied images.
character-charts:
	$(PYTHON) $(CHART_DIR)/generate.py

install-character-charts:
	mkdir -p SDDStoolkit/$(CHART_DIR)
	cp $(CHART_DIR)/index.html $(CHART_DIR)/*.png $(CHART_DIR)/sddsplot-character-charts.zip SDDStoolkit/$(CHART_DIR)/

SDDStoolkit.ps:
	rm -f SDDStoolkit.aux SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.out
	latex -interaction=nonstopmode SDDStoolkit || true
	latex -interaction=nonstopmode SDDStoolkit || true
	dvips SDDStoolkit -o SDDStoolkit.ps
	rm -f SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.aux SDDStoolkit.out

SDDStoolkit.pdf: SDDStoolkit.ps
	ps2pdf13 SDDStoolkit.ps SDDStoolkit.pdf

html:
	mkdir -p SDDStoolkit
	htlatex SDDStoolkit "html,3" "" "-dSDDStoolkit/"
	$(MAKE) install-character-charts
	rm -f *.html *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

clean::
	rm -f SDDStoolkit.aux SDDStoolkit.dvi SDDStoolkit.log SDDStoolkit.toc SDDStoolkit.ps SDDStoolkit.pdf
	rm -rf SDDStoolkit
