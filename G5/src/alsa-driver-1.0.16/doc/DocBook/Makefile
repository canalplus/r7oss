TOPDIR=../..

DOCBOOKS := alsa-driver-api.sgml writing-an-alsa-driver.sgml

include $(TOPDIR)/Makefile.conf

TOPDIR = $(MAINSRCDIR)
CURDIR = $(subst $(MAINSRCDIR)/,,$(shell /bin/pwd))

# The targets that may be used.
.PHONY:	depend sgmldocs psdocs pdfdocs htmldocs clean mrproper

BOOKS := $(DOCBOOKS)
sgmldocs: $(BOOKS)

PS := $(patsubst %.sgml, %.ps, $(BOOKS))
psdocs: $(PS) $(DOCPROC)

PDF := $(patsubst %.sgml, %.pdf, $(BOOKS))
pdfdocs: $(PDF) $(DOCPROC)

HTML := $(patsubst %.sgml, %, $(BOOKS))
htmldocs: $(HTML) $(DOCPROC)

###
# non-standard rules below to simplify...

KERNELDOC=$(TOPDIR)/scripts/kernel-doc
DOCPROC=$(TOPDIR)/scripts/docproc

$(DOCPROC):
	$(MAKE) -C $(TOPDIR)/scripts docproc

%.tmpl:
	ln -s $(TOPDIR)/alsa-kernel/Documentation/DocBook/$@

%.sgml: %.tmpl
	(cd $(TOPDIR);\
	 $(DOCPROC) doc $(CURDIR)/$< > $(CURDIR)/$@)

###

DVI	:=	$(patsubst %.sgml, %.dvi, $(BOOKS))
AUX	:=	$(patsubst %.sgml, %.aux, $(BOOKS))
TEX	:=	$(patsubst %.sgml, %.tex, $(BOOKS))
LOG	:=	$(patsubst %.sgml, %.log, $(BOOKS))
OUT	:=	$(patsubst %.sgml, %.out, $(BOOKS))

clean:
	rm -f core *~
	rm -f $(BOOKS) $(patsubst %.sgml, %.tmpl, $(BOOKS))
	rm -f $(DVI) $(AUX) $(TEX) $(LOG) $(OUT)
	rm -f $(PNG-parportbook) $(EPS-parportbook)
	rm -f $(C-procfs-example)

mrproper: clean
	rm -f $(PS) $(PDF)
	rm -f -r $(HTML)
	rm -f .depend
	rm -f $(TOPDIR)/scripts/mkdep-docbook
	rm -rf DBTOHTML_OUTPUT*


JADE     = openjade
JADETEX  = jadetex
PDFJADETEX  = pdfjadetex
DVIPS	 = dvips

DSLBASE  = /usr/share/sgml
JADECTL  = $(DSLBASE)/openjade/catalog
DOCBKCTL = $(DSLBASE)/CATALOG.docbook_4
DOCBKPRN = $(DSLBASE)/docbook/dsssl-stylesheets/print/docbook.dsl
DOCBKHTM = $(DSLBASE)/docbook/dsssl-stylesheets/html/docbook.dsl
JADEX    = $(JADE) -c $(DOCBKCTL) -c $(JADECTL)

%.html: %.sgml
	$(JADEX) -d $(DOCBKHTM) -t sgml $<

%.tex: %.sgml
	$(JADEX) -d $(DOCBKPRN) -t tex $<

%.css: %.sgml
	$(JADEX) -d $(DOCBKHTM) -t html -o $@ $<

%.dvi: %.tex
	$(JADETEX) $< && $(JADETEX) $< && $(JADETEX) $<

%.pdf: %.tex
	$(PDFJADETEX) $< && $(PDFJADETEX) $< && $(PDFJADETEX) $<

%.ps: %.dvi
	$(DVIPS) $<

%: %.sgml
	rm -rf $@
	mkdir $@
	(cd $@;\
	$(JADEX) -d $(DOCBKHTM) -t sgml ../$<)
