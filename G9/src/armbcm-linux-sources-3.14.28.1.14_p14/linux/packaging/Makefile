# update the 'Version:' tag in the .spec file

THE_MAKEFILE := ../Makefile
V1 := ${shell head -10 ${THE_MAKEFILE} | grep -v "^\#" | grep -w VERSION | cut -d= -f2 | grep -o "[0-9]*"}
V2 := ${shell head -10 ${THE_MAKEFILE} | grep -v "^\#" | grep -w PATCHLEVEL | cut -d= -f2 | grep -o "[0-9]*"}
V3 := ${shell head -10 ${THE_MAKEFILE} | grep -v "^\#" | grep -w EXTRAVERSION | cut -d= -f2 | sed "s/pre.*//" | grep -o "[0-9.]*"}

all: update_version_tag

.PHONY : update_version_tag
update_version_tag : linux-src.spec
	@sed -i -e "s/^\(Version:.*\s\).*$$/\1$(V1).$(V2).$(V3)/" $<
	@grep "^Version:" $<
