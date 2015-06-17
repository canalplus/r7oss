CC=gcc
ODIR=obj
DRVDIR=mpe42
SRC_TOPDIR=../../../..
STG_TOPDIR=$(SRC_TOPDIR)
STMFB=$(SRC_TOPDIR)
FVDP=$(SRC_TOPDIR)/fvdp
INC=-I${SRC_TOPDIR}/include -I${SRC_TOPDIR}/private/include
RUNTIME_PLATFORM=PLATFORM_LINUX
DEFINES:=-DFVDP_SIMULATION -Wall -I$(FVDP) -I$(FVDP)/$(DRVDIR) -DDEBUG -DVCD_OUTPUT_ENABLE=TRUE
DEFINES+=-DNEW_LATENCY_DEFINITION
GDBDEBUG=-g3 -O0
CONFIG_MPE42:=y
CONFIG_FVDP=y
include $(SRC_TOPDIR)/fvdp/fvdp.mak
IGNORE_FILES += $(addprefix $(SRC_TOPDIR)/fvdp/,FvdpPlane.cpp)
STM_SRC_FILES += fvdp_sim.o

CFLAGS=$(INC) $(DEFINES) $(GDBDEBUG) $(EXTRA_CFLAGS) -m32
obj_branch := $(patsubst $(SRC_TOPDIR)/%.c,%.c,$(filter-out $(IGNORE_FILES),$(STM_SRC_FILES)))
objects := $(patsubst %.c,obj/%.o,$(obj_branch))

all: $(objects)
	$(CC) $(objects) $(CFLAGS) -lpthread -lreadline -o sim.out

clean:
	rm -f $(objects)
	rm -rf obj
	rm -f sim.out

run:
	gdb ./sim.out -ex r

$(objects): | obj | obj/fvdp | obj/fvdp/$(DRVDIR)

obj:
	@mkdir -p $@

obj/fvdp:
	@mkdir -p $@

obj/fvdp/$(DRVDIR):
	@mkdir -p $@

obj/%.o: $(SRC_TOPDIR)/%.c
	$(CC) $< -o $@ -c $(INC) $(CFLAGS)

.DEFAULT: ;

