# This makefile is not used by the glibc build process.
# It's purely for making ports tarballs.

.PHONY: dist dist-ports
dist: dist-ports

# Do `make dist dist-version=X.Y.Z' to make tar files of an older version.
dist-version = $(shell git describe --abbrev=0  | awk -F '-' '{print $$4}')
# Also try 'dist-tag=some="-r TAG"' (or -D DATE) to get some tag rather
# than the release tag for X.Y.Z.
dist-tag = -r glibc-$(subst .,_,$(dist-version))

distname = glibc-ports-$(dist-version)

do-export = cvs $(CVSOPTS) -Q export -d $(basename $@) $(dist-tag)

dist-ports: $(foreach Z,.bz2 .gz,$(distname).tar$Z)
	md5sum $^

force: ;

$(distname).tar: force
	@rm -fr $(basename $@)
	$(MAKE) -q `find sysdeps -name configure`
	rm -f $@ $@.new $@.bz2 $@.gz
	git archive --prefix=$(distname)/ HEAD > $@.new
	mv -f $@.new $@


sysdeps-of-stem = sysdeps/$* sysdeps/unix/sysv/linux/$*

.PRECIOUS: %.gz %.bz2 # Don't delete output as intermediate files.
dist-port-%: $(foreach Z,.bz2 .gz,glibc-port-%-$(dist-version).tar$Z)
	md5sum $^
glibc-port-%-$(dist-version).tar: ChangeLog.%
	@rm -fr $(basename $@)
	$(MAKE) -q `find $(sysdeps-of-stem) -name configure`
	$(do-export) ports/ChangeLog.$* $(addprefix ports/,$(sysdeps-of-stem))
	mv $(basename $@)/ports/* $(basename $@)/
	rmdir $(basename $@)/ports
	find $(basename $@) -name configure -print | xargs touch
	tar cf $@ $(basename $@)
	rm -fr $(basename $@)

%.bz2: %; bzip2 -9vk $<
%.gz: %; gzip -9vnc $< > $@.new && mv -f $@.new $@

changelog:
	$(Q)echo -e "-----------------------------------------------------------------" > glibc-ports-$(dist-version).changelog.x
	$(Q)echo "*" `date +"%a %b %e %Y"`" - Release: "`git describe --abbrev=0` >> glibc-ports-$(dist-version).changelog.x
	$(Q)echo -e "-----------------------------------------------------------------\n" >> glibc-ports-$(dist-version).changelog.x
	$(Q)set -- `git for-each-ref --sort=-taggerdate --format='%(refname:short)' refs/tags/stlinux2.4* \
	| grep -v rc | head -n 2` && echo "Creating the incremental changelog: $${2} --> $${1}" && \
	git log --no-merges $${2}..$${1} >> glibc-ports-$(dist-version).changelog.x
