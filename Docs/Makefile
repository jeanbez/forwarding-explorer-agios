LATEX = pdflatex -halt-on-error -file-line-error
BIBTEX = bibtex
MAINFILE = agios_doc


#Listas de targets pdfs que devem ser gerados apartir dos fontes em outros formatos

rawimgs := $(wildcard images/*)

bibs    := $(wildcard *.bib)

all: $(MAINFILE).pdf

subdirs:
	@for i in $(SUBDIRS); do \
		echo "make all in $$i..."; \
		(cd $$i; $(MAKE) $(MFLAGS) $(MYMAKEFLAGS) all); done

#all: press

view: $(MAINFILE).pdf sections/*.tex
	evince $(MAINFILE).pdf

svg/%.pdf: svg/%.svg
	inkscape -z -f $< --export-area-snap  -E /dev/stdout|epstopdf --filter --outfile=$@

#java -jar batik/batik-rasterizer.jar -m application/pdf -d svg $<

eps/%.pdf: eps/%.eps
	epstopdf --outfile=$@ $<

fig/%.pdf: fig/%.fig
	fig2dev -L eps $<|epstopdf --filter --outfile=$@

$(MAINFILE).pdf: *.tex $(bibs) $(rawimgs)
	$(LATEX) $(MAINFILE)
	$(BIBTEX) $(MAINFILE) || echo BIBTEX SUCKS
	$(LATEX) $(MAINFILE)
	$(BIBTEX) $(MAINFILE) || echo BIBTEX SUCKS
	$(LATEX) $(MAINFILE)
	$(LATEX) $(MAINFILE)



*.ps: *.pdf
	pdf2ps -dLanguageLevel=2 $(MAINFILE).pdf -o $(MAINFILE).ps




.PHONY: clean
clean:
	rm -f *.pdf *.ps svg/*.pdf eps/*.pdf fig/*.pdf *.bbl *.aux *.blg *.log *.lot *.lof || echo "Clean"

soft_clean:
	rm -f *.pdf *.ps *.pdf *.bbl *.aux *.blg *.log *.lot *.lof || echo "Clean"

