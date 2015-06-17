# Create the target lists
SOURCES := $(wildcard *.diag)
TARGETS := $(patsubst %.diag,%.png,$(SOURCES))
#TARGETS += $(patsubst %.diag,%.svg,$(SOURCES))

OPTS := --no-transparency

all: $(TARGETS)

SEQDIAG:=$(shell which seqdiag)
ifeq ($(SEQDIAG),)
@$(error "SeqDiag is required for this tool to run : http://blockdiag.com/en/seqdiag/introduction.html#setup")
endif

%.png: %.diag
	$(SEQDIAG) $(OPTS) $<

%.svg: %.diag
	$(SEQDIAG) -Tsvg $<

clean:
	rm -f *.png *.svg
