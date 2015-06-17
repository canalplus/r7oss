#
# This file contains rules which are shared between multiple Makefiles.
#
# It's a stripped and modified version of /usr/src/linux/Rules.make. [--jk]
#

MODCURDIR = $(subst $(MAINSRCDIR)/,,$(shell /bin/pwd))

ifdef NEW_KBUILD

ifdef KBUILD_MODULES

# clean obsolete definitions
export-objs :=

# apply patches beforehand
.PHONY: cleanup
cleanup:
	rm -f *.[oas] *.ko .*.cmd .*.d .*.tmp *.mod.c $(clean-files)
	@for d in $(patsubst %/,%,$(filter %/, $(obj-y))) \
	          $(patsubst %/,%,$(filter %/, $(obj-m))) DUMMY; do \
	 if [ $$d != DUMMY ]; then $(MAKE) -C $$d cleanup; fi; \
	done

else # ! KBUILD_MODULES

first_rule: modules

include $(MAINSRCDIR)/Rules.make1

%.c: %.patch
	@SND_TOPDIR="$(MAINSRCDIR)" $(SND_TOPDIR)/utils/patch-alsa $@

# apply patches beforehand
.PHONY: prepare
prepare: $(clean-files)
	@for d in $(ALL_SUB_DIRS) DUMMY; do \
	 if [ $$d != DUMMY ]; then $(MAKE) -C $$d prepare; fi; \
	done

modules:
	$(MAKE) prepare
	$(MAKE) -C $(CONFIG_SND_KERNELDIR) SUBDIRS=$(MAINSRCDIR) $(MAKE_ADDS) SND_TOPDIR=$(MAINSRCDIR) modules
	$(SND_TOPDIR)/utils/link-modules $(MODCURDIR)

ALL_MOBJS := $(filter-out $(obj-y), $(obj-m))
ALL_MOBJS := $(filter-out %/, $(ALL_MOBJS))
modules_install:
ifneq "$(strip $(ALL_MOBJS))" ""
	mkdir -p $(DESTDIR)$(moddir)/$(MODCURDIR)
	cp $(sort $(ALL_MOBJS:.o=.ko)) $(DESTDIR)$(moddir)/$(MODCURDIR)
endif
	@for d in $(ALL_SUB_DIRS) DUMMY; do \
	 if [ $$d != DUMMY ]; then $(MAKE) -C $$d modules_install; fi; \
	done

endif # KBUILD_MODULES

else # ! NEW_KBUILD

TOPDIR = $(MAINSRCDIR)

comma = ,

#
# False targets.
#
.PHONY: dummy

#
# Get things started.
#
first_rule: modules

include $(TOPDIR)/Rules.make1

ifndef O_TARGET
ifndef L_TARGET
O_TARGET	:= built-in.o
endif
endif

#
# Common rules
#

%.s: %.c
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS)  -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) -S $< -o $@

%.i: %.c
	$(CPP) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) -D"KBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) $(CFLAGS_$@) $< > $@

%.o: %.c
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) $(CFLAGS_$@) -c -o $@ $<

%.o: %.s
	$(AS) -D__KERNEL__ $(AFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# Old makefiles define their own rules for compiling .S files,
# but these standard rules are available for any Makefile that
# wants to use them.  Our plan is to incrementally convert all
# the Makefiles to these standard rules.  -- rmk, mec
ifdef USE_STANDARD_AS_RULE

%.s: %.S
	$(CPP) -D__KERNEL__ $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) $< > $@

%.o: %.S
	$(CC) -D__KERNEL__ $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<

endif

%.c: %.patch
	@xtmp=`echo $(MODCURDIR) | sed -e 's/^acore/core/'`/$@;\
	echo "copying file alsa-kernel/$$xtmp";\
	cp "$(TOPDIR)/alsa-kernel/$$xtmp" $@;\
	patch -p0 -i $<

%.isapnp: %.c
ifeq (y,$(CONFIG_ISAPNP))
	$(CPP) -C -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) -D__isapnp_now__ -DKBUILD_BASENAME=$(subst $(comma),_,$(subst -,_,$(*F))) $(CFLAGS_$@) $(CFLAGS_$@) $< | awk -f $(TOPDIR)/utils/convert_isapnp_ids > $@
else
	rm -f $@
	touch $@
endif

#
#
#
all_targets: $(isapnp-files) $(O_TARGET) $(L_TARGET)

#
# Rule to compile a set of .o files into one .o file
#
ifdef O_TARGET
$(O_TARGET): $(obj-y)
	touch $@
endif # O_TARGET

#
# Rule to compile a set of .o files into one .a file
#
ifdef L_TARGET
$(L_TARGET): $(obj-y)
	touch $@
endif

#
# Rule to link composite objects
#

__obj-m = $(filter-out export.o,$(obj-m))
ld-multi-used-m := $(sort $(foreach m,$(__obj-m),$(patsubst %,$(m),$($(basename $(m))-objs) $($(basename $(m))-y))))
ld-multi-objs-m := $(foreach m, $(ld-multi-used-m), $($(basename $(m))-objs) $($(basename $(m))-y) $(extra-$(basename $(m))-objs))

depend-objs	:= $(foreach m,$(__obj-m),$($(basename $(m))-objs) $($(basename $(m))-y))
depend-files	:= $(patsubst %.o,%.c,$(depend-objs))

$(ld-multi-used-m) : %.o: $(ld-multi-objs-m)
	rm -f $@
	$(LD) $(EXTRA_LDFLAGS) -r -o $@ $(filter $($(basename $@)-objs) $($(basename $@)-y) $(extra-$(basename $@)-objs), $^)

#
# This make dependencies quickly
#
fastdep: $(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)) update-sndversions $(depend-files)
ifneq "$(strip $(depend-files))" ""
		$(CC) -M -D__KERNEL__ -D__isapnp_now__ $(CPPFLAGS) $(EXTRA_CFLAGS) $(depend-files) > .depend
endif

ifneq "$(strip $(ALL_SUB_DIRS))" ""
$(patsubst %,_sfdep_%,$(ALL_SUB_DIRS)):
	$(MAKE) -C $(patsubst _sfdep_%,%,$@) fastdep
endif

#
# A rule to make subdirectories
#
subdir-list = $(sort $(patsubst %,_subdir_%,$(SUB_DIRS)))
sub_dirs: dummy $(subdir-list)

ifdef SUB_DIRS
$(subdir-list) : dummy
	$(MAKE) -C $(patsubst _subdir_%,%,$@)
endif

#
# A rule to make modules
#
ALL_MOBJS = $(filter-out $(obj-y), $(obj-m))

MOD_DIRS := $(MOD_SUB_DIRS) $(MOD_IN_SUB_DIRS)
ifneq "$(strip $(MOD_DIRS))" ""
.PHONY: $(patsubst %,_modsubdir_%,$(MOD_DIRS))
$(patsubst %,_modsubdir_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modsubdir_%,%,$@) modules

.PHONY: $(patsubst %,_modinst_%,$(MOD_DIRS))
$(patsubst %,_modinst_%,$(MOD_DIRS)) : dummy
	$(MAKE) -C $(patsubst _modinst_%,%,$@) modules_install
endif

.PHONY: modules
modules: $(isapnp-files) $(ALL_MOBJS) dummy \
	 $(patsubst %,_modsubdir_%,$(MOD_DIRS))

.PHONY: _modinst__
_modinst__: dummy
ifneq "$(strip $(ALL_MOBJS))" ""
ifeq ($(moddir_tree),y)
	mkdir -p $(DESTDIR)$(moddir)/$(MODCURDIR)
	cp $(sort $(ALL_MOBJS)) $(DESTDIR)$(moddir)/$(MODCURDIR)
else
	mkdir -p $(DESTDIR)$(moddir)
	cp $(sort $(ALL_MOBJS)) $(DESTDIR)$(moddir)
endif
endif

.PHONY: modules_install
modules_install: _modinst__ \
	 $(patsubst %,_modinst_%,$(MOD_DIRS))

#
# This sets version suffixes on exported symbols
# Separate the object into "normal" objects and "exporting" objects
# Exporting objects are: all objects that define symbol tables
#
ifdef CONFIG_MODULES

ifeq (y,$(CONFIG_SND_MVERSION))
ifneq "$(strip $(export-objs))" ""

MODINCL = $(TOPDIR)/include/modules
MODPREFIX = $(subst /,-,$(MODCURDIR))__

# The -w option (enable warnings) for genksyms will return here in 2.1
# So where has it gone?
#
# Added the SMP separator to stop module accidents between uniprocessor
# and SMP Intel boxes - AC - from bits by Michael Chastain
#

ifdef $(msmp)
	genksyms_smp_prefix := -p smp_
else
	genksyms_smp_prefix := 
endif

$(MODINCL)/$(MODPREFIX)%.ver: %.c update-sndvers
	@if [ ! -r $(MODINCL)/$(MODPREFIX)$*.stamp -o $(MODINCL)/$(MODPREFIX)$*.stamp -ot $< ]; then \
		if [ ! -f $(CONFIG_SND_KERNELDIR)/include/linux/modules/$*.stamp ]; then \
		echo '$(CC) -D__KERNEL__ $(CPPFLAGS) $(EXTRA_CFLAGS) -E -D__GENKSYMS__ $<'; \
		echo '| $(GENKSYMS) $(genksyms_smp_prefix) > $@.tmp'; \
		$(CC) -D__KERNEL__ $(CPPFLAGS) $(EXTRA_CFLAGS) -E -D__GENKSYMS__ $< \
		| $(GENKSYMS) $(genksyms_smp_prefix) > $@.tmp; \
		if [ -r $@ ] && cmp -s $@ $@.tmp; then echo $@ is unchanged; rm -f $@.tmp; \
		else echo mv $@.tmp $@; mv -f $@.tmp $@; fi; \
		elif [ ! -r $@ ]; then touch $@; \
		fi; \
	fi; touch $(MODINCL)/$(MODPREFIX)$*.stamp

$(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver)): $(TOPDIR)/include/config.h $(TOPDIR)/include/config1.h

# updates .ver files but not modversions.h
fastdep: $(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver))

endif # export-objs 

define update-sndvers
	(tmpfile=`echo $(SNDVERSIONS).tmp`; \
	(echo "#ifndef _LINUX_SNDVERSIONS_H"; \
	  echo "#define _LINUX_SNDVERSIONS_H"; \
	  echo "#include <linux/modsetver.h>"; \
	  cd $(TOPDIR)/include/modules; \
	  for f in *.ver; do \
	    if [ -f $$f ]; then echo "#include \"modules/$${f}\""; fi; \
	  done; \
	  echo "#endif"; \
	) > $${tmpfile}; \
	if [ -r $(SNDVERSIONS) ] && cmp -s $(SNDVERSIONS) $${tmpfile}; then \
		echo $(SNDVERSIONS) was not updated; \
		rm -f $${tmpfile}; \
	else \
		echo $(SNDVERSIONS) was updated; \
		mv -f $${tmpfile} $(SNDVERSIONS); \
	fi)
endef

$(SNDVERSIONS):
	$(update-sndvers)

$(active-objs): $(SNDVERSIONS)

else # !CONFIG_SND_MVERSION

define update-sndvers
	@echo "" > $(SNDVERSIONS)
endef

$(SNDVERSIONS):
	$(update-sndvers)

endif # CONFIG_SND_MVERSION

$(ld-multi-used-m): $(addprefix $(TOPDIR)/modules/,$(ld-multi-used-m))

$(TOPDIR)/modules/%.o: dummy
	@if ! test -L $@; then \
	    echo "ln -sf ../$(MODCURDIR)/$(notdir $@) $(TOPDIR)/modules/$(notdir $@)" ; \
	    ln -sf ../$(MODCURDIR)/$(notdir $@) $(TOPDIR)/modules/$(notdir $@) ; \
	fi

.PHONY: update-sndversions
update-sndversions: dummy
	$(update-sndvers)

ifneq "$(strip $(export-objs))" ""
ifeq (y,$(CONFIG_SND_MVERSION))
$(export-objs): $(addprefix $(MODINCL)/$(MODPREFIX),$(export-objs:.o=.ver)) $(export-objs:.o=.c)
else
$(export-objs): $(export-objs:.o=.c)
endif
	$(CC) -D__KERNEL__ $(CFLAGS) $(EXTRA_CFLAGS) $(CFLAGS_$@) -DEXPORT_SYMTAB -c $(@:.o=.c)
endif

endif # CONFIG_MODULES

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif

ifneq ($(wildcard $(TOPDIR)/.hdepend),)
include $(TOPDIR)/.hdepend
endif

endif	# NEW_KBUILD
