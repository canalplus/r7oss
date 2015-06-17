all : test doxygen

clean :
	$(MAKE) -C ../images clean
	$(RM) -r html latex $(TESTS) se_api.pdf

DEPS = Makefile Doxyfile $(wildcard ../../player/include*.h ../../player/include/stm_se/*.h ../pages/*.h)


doxygen : images html/index.html latex/refman.pdf

images :
	$(MAKE) -C ../images

html/index.html : $(DEPS)
	test `doxygen --version | cut -f1 -d.` -eq 1
	test `doxygen --version | cut -f2 -d.` -ge 8
	doxygen
	./textune.py --promote latex/group_*.tex || ($(RM) $@; false)
	./textune.py --organize latex/refman.tex || ($(RM) $@; false)
	./textune.py --stylize latex/doxygen.sty || ($(RM) $@; false)

latex/refman.pdf : html/index.html $(DEPS)
	make -C latex
	test -e se_api.pdf || ln -s latex/refman.pdf se_api.pdf

# Verify the headers are compilable
TESTS = test_se.o test_internal.o
INFRASTRUCTURE ?= /opt/STM/STLinux-2.4/devkit/sources/infrastructure/infrastructure
CFLAGS = -Wall -Werror -I$(INFRASTRUCTURE)/include -I../../player/include
test : $(TESTS)
$(TESTS) : $(DEPS)
