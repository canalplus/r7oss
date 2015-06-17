SVGFILES = $(wildcard *.svg)
PDFFILES = $(SVGFILES:.svg=.pdf)

all : $(PDFFILES)

clean :
	$(RM) $(PDFFILES)

%.pdf : %.svg
	inkscape --without-gui --export-pdf=$@ $<
