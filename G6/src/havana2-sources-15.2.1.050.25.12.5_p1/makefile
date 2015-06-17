ifeq ($(RPMIT),y)
	@echo "NO RPM GENERATOR"
else

## O=<objects> ODIR2=/full/path/to/root/
## V=1    show complete command
## VV=1   show GROUP and CD directives

## CDIR   source directory (used by makefiles for including local files)

ROOT_DIR?=$(shell pwd)
SUB_DIR?=./
NEWPATH?=

NDIR?=$(shell pwd)/

CDIR=$(ROOT_DIR)/$(NEWPATH)

ifeq ($(O),)
# if O= is not specified
OUTPUT:=$(ARCH)
ODIR2?=$(PWD)/$(OUTPUT)objs
else
# if O= is specified
ODIR3:=$(filter /%,$(O))
ifeq ($(ODIR3),)
# if O= does not start with an absolute path, prepend PWD
ODIR2?=$(PWD)/$(O)
else
# if O= specifies an absolute path, use it
ODIR2?=$(O)
endif
endif

ARCH ?= $(shell uname -i)

## We need rules to be included but variables not to be (like CFLAGS)!!!
-include $(ROOT_DIR)/makefile.$(ARCH)
include $(NDIR)/makefile.se

ODIR:=$(ODIR2)/$(NEWPATH)

RUN?=no

export CFLAGS ROOT_DIR ARFLAGS LDFLAGS V VV ODIR2 RUN ARCH

ifeq ($(RUN),no)
RUN:=yes
all:
	mkdir -p $(ODIR)
	make -s -C $(ODIR) -f $(ROOT_DIR)/makefile NDIR=$(NDIR)

clean: 
	rm -fr $(ODIR)
else

MAKEDEP := $(CC) -M -MG
ARFLAGS := rcs

subdirs      = $(filter %/,$(targets))
objects      = $(filter-out %/,$(targets))
real_targets = $(objects)
#source_dirs  = $(addprefix $(ROOT_DIR)/,$(foreach f,$(real_targets),$(dir $(f))))


##TT:=$(filter-out %/,$(obj-y))
ifneq ($(obj-y),)
real_targets+=builtin.o
endif

dirbuiltins  = $(addsuffix builtin.o, $(subdirs))

obj_targets  = $(dirbuiltins) $(filter-out %/,$(obj-y))

libraries-a  = $(filter %.a,$(libraries))
libraries-so = $(filter %.so,$(libraries))

targets      = $(libraries-a) $(libraries-so) $(applications-cc) $(applications-cxx) $(obj-y)
##cmd_files    = $(foreach f,$(real_targets),$(dir $(f))$(notdir $(f)).cmd)
cmd_files    = $(foreach f,$(real_targets),$(notdir $(f)).cmd)

CC_LINKER_FLAG=-Wl,

ifeq ($(V),)
quiet=quiet_
endif


quiet_cmd_cc         = CC       $@
      cmd_cc         = $(CC)    $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

quiet_cmd_cxx        = CXX      $@
      cmd_cxx        = $(CXX)    $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

quiet_cmd_ld         = LD       $@
      cmd_ld         = $(LD)    $< $(LDFLAGS) $(LDFLAGS_$@) -o $@

quiet_cmd_app_cc     = APP-CC   $@
      cmd_app_cc     = $(CC)    $(PRE_LDFLAGS) $^ $(addprefix $(CC_LINKER_FLAG),$(LDFLAGS)) $(addprefix $(CC_LINKER_FLAGS),$(LDFLAGS_$@)) -o $@

quiet_cmd_app_cxx    = APP-CXX  $@ from $^
      cmd_app_cxx    = $(CXX)   $(PRE_LDFLAGS) -Wl,--start-group $^ $(addprefix $(CC_LINKER_FLAG),$(LDFLAGS)) $(addprefix $(CC_LINKER_FLAGS),$(LDFLAGS_$@)) -Wl,--end-group $(POST_LDFLAGS) -o $@

ifneq ($(VV),)
quiet_cmd_group      = GROUP    ($(PWD))/$@
endif
      cmd_group      = $(LD)    -r --start-group $^ --end-group $(LDFLAGS) $(LDFLAGS_$@) -o $@

quiet_cmd_ar         = AR       $@
      cmd_ar         = $(AR)    $(ARFLAGS) $(ARFLAGS_$@) $@.a $^

quiet_cmd_so         = SO       $@
      cmd_so         = $(LD)    $(LDFLAGS) $(LDFLAGS_$@) -shared -o $@ --start-group $^ --end-group

ifneq ($(VV),)
quiet_cmd_makedep_cc = MAKEDEP  CC $@ from $<
endif
      cmd_makedep_cc = $(MAKEDEP) $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

ifneq ($(VV),)
quiet_cmd_makedep_cxx= MAKEDEP  CXX $@ from $<
endif
      cmd_makedep_cxx= $(MAKEDEP) $(CFLAGS) $(CFLAGS_$@) -c $< -o $@

ifneq ($(VV),)
quiet_cmd_makedep_o  = MAKEDEP  O $@ from $*
endif
      cmd_makedep_o  = echo $*: $(filter-out %/,$($*-objs)) $(addsuffix builtin.o, $(filter %/,$($*-objs))) > $@; echo targets+= $($*-objs)  >> $@; 

ifneq ($(VV),)
quiet_cmd_makedep_so = MAKEDEP  SO $@ from $*
endif
      cmd_makedep_so = echo $*: $(filter-out %/,$($*-objs)) $(addsuffix builtin.o, $(filter %/,$($*-objs))) > $@; echo targets+= $($*-objs)  >> $@; 

ifneq ($(VV),)
quiet_cmd_makedep_a  = MAKEDEP  A $@ from $*
endif
      cmd_makedep_a  = echo $*.a: $(filter-out %/,$($*-objs)) $(addsuffix builtin.o, $(filter %/,$($*-objs))) > $@; echo targets+= $($*-objs)  >> $@; 

ifneq ($(VV),)
quiet_cmd_recurse    = CD       $(shell pwd)/$*
endif
      cmd_recurse    = make -s -C $(2) -f $(ROOT_DIR)/makefile NEWPATH=$(NEWPATH)/$(2) NDIR=$(NDIR)/$(2)

# Convinient variables
comma   := ,
squote  := '                    #'
empty   :=
space   := $(empty) $(empty)



escsq = $(subst $(squote),'\$(squote)',$1)

cmd = @$(echo-cmd) $(cmd_$(1))

echo-cmd = $(if $($(quiet)cmd_$(1)), \
        echo '  $(call escsq,$($(quiet)cmd_$(1)))';)


THE_MAKEFILES=$(NDIR)/makefile.se $(ROOT_DIR)/makefile

all:

VPATH:=$(NDIR) $(source_dirs)

%/builtin.o: .PHONY
	$(call cmd,recurse,$*)

.PHONY: %/builtin.o

builtin.o:
	$(call cmd,group)

builtin.o.cmd: $(THE_MAKEFILES)
	@echo builtin.o: $(obj_targets) > $@

##%.o: %.o
##	echo here $^ $@ $<
##	$(call cmd,group)


-include $(cmd_files)

pclean:
	find -name "*.o" | xargs rm



## Create relative directories only if they do not exist
## ie contianers/ src/common.o are created (ie. $(ODIR)/containers $(ODIR)/src)
## but /direct/path/to/file is not (ie. $(ODIR)/direct/path/to/file)
obj-dirs := $(dir $(addprefix $(ODIR)/,$(filter-out /%,$(dir-y) $(targets))))
_dummy := $(foreach d,$(obj-dirs), $(shell [ -d $(d) ] || mkdir -p $(d)))

## Add this either to the beginning or end of the 'all' dependency rule
debug:
	@echo NDIR:$(NDIR)
	@echo NEWPATH:$(NEWPATH)
	@echo obj-dirs:$(obj-dirs)
	@echo cmd_files:$(cmd_files)
	@echo real_targets:$(real_targets) 
	@echo targets:$(targets)
	@echo subdirs:$(subdirs)
	@echo objects:$(objects)
	@echo applications-cc:$(applications-cc)
	@echo applications-cxx:$(applications-cxx)
	@echo libraries-a:$(libraries-a)
	@echo libraries-so:$(libraries-so)
	@echo ODIR:$(ODIR)
	@echo pwd:$(PWD)
	@echo $(THE_MAKEFILES)
	@echo source_dirs:$(source_dirs)


$(applications-cc):
	$(call cmd,app_cc)

$(applications-cxx):
	$(call cmd,app_cxx)

$(libraries-a):
	$(call cmd,ar)

$(libraries-so):
	$(call cmd,so)

%.o.cmd: %.c $(THE_MAKEFILES)
	$(call cmd,makedep_cc)

%.o.cmd: %.cpp $(THE_MAKEFILES)
	$(call cmd,makedep_cxx)

##tmp.o.cmd: /home/damia/users/bennettp/sources/havana/com.st.player2.bug_1421/player/tmp.cpp
##	$(call cmd,makedep_cxx)

%.cmd: %.a $(THE_MAKEFILES)
	$(call cmd,makedep_a)

%.cmd: %.so $(THE_MAKEFILES)
	$(call cmd,makedep_so)

%.cmd: $(THE_MAKEFILES)
	$(call cmd,makedep_o)

%.o: %.c $(THE_MAKEFILES)
	$(call cmd,cc)

%.o: %.cpp $(THE_MAKEFILES)
	$(call cmd,cxx)


all: $(dir-y) $(subdirs) $(cmd_files) $(real_targets) $(THE_MAKEFILES)


$(dir-y): .PHONY
	$(call cmd,recurse,$@)



endif

endif
