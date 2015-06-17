## $Id: Makefile.am,v 5.73 2010/05/13 16:23:23 lirc Exp $

## Process this file with automake to produce Makefile.in 

## these are used by programs in the tools directory, but not by
## programs in this directory.  So we must make sure they get distributed

INCLUDES = -I$(top_srcdir)

BUILT_SOURCES = input_map.inc

noinst_LIBRARIES = libhw_module.a
libhw_module_a_SOURCES = \
			hw-types.c hw-types.h hardware.h \
			ir_remote.c ir_remote.h ir_remote_types.h \
			release.c release.h

EXTRA_libhw_module_a_SOURCES = \
			hw_accent.c hw_accent.h \
			hw_alsa_usb.c \
			hw_atilibusb.c \
			hw_atwf83.c \
			hw_audio.c \
			hw_audio_alsa.c \
			hw_awlibusb.c \
			hw_bte.c hw_bte.h \
			hw_caraca.c hw_caraca.h \
			hw_commandir.c hw_commandir.h \
			hw_creative.c hw_creative.h \
			hw_creative_infracd.c hw_creative_infracd.h \
			hw_default.c hw_default.h \
			hw_devinput.c \
			hw_dfclibusb.c \
			hw_dsp.c \
			hw_ea65.c hw_ea65.h \
			hw_ftdi.c \
			hw_hiddev.c \
			hw_i2cuser.c \
			hw_iguanaIR.c \
			hw_irlink.c hw_irlink.h \
			hw_irman.c hw_irman.h \
			hw_livedrive_common.c hw_livedrive_common.h \
			hw_livedrive_midi.c hw_livedrive_seq.c \
			hw_logitech.c hw_logitech.h \
			hw_mouseremote.c hw_mouseremote.h \
			hw_mp3anywhere.c hw_mp3anywhere.h \
			hw_mplay.c hw_mplay.h \
			hw_pcmak.c hw_pcmak.h \
			hw_pinsys.c hw_pinsys.h \
			hw_pixelview.c hw_pixelview.h \
			hw_silitek.c hw_silitek.h \
			hw_slinke.c hw_slinke.h \
			hw_srm7500libusb.c hw_srm7500libusb.h \
			hw_tira.c hw_tira.h \
			hw_udp.c \
			hw_uirt2.c hw_uirt2_raw.c \
			hw_uirt2_common.c hw_uirt2_common.h \
			hw_usbx.c hw_usbx.h \
			receive.c receive.h \
			transmit.c transmit.h \
			serial.c serial.h

libhw_module_a_LIBADD = @hw_module@
libhw_module_a_DEPENDENCIES = @hw_module@

sbin_PROGRAMS = lircd lircmd

lircd_SOURCES = lircd.c lircd.h \
		config_file.c config_file.h \
		input_map.c input_map.h \
		transmit.c transmit.h
lircd_LDADD = @daemon@ libhw_module.a @hw_module_libs@

lircmd_SOURCES = lircmd.c
lircmd_LDADD = @daemon@

bin_PROGRAMS = irrecord

irrecord_SOURCES = irrecord.c \
		config_file.c config_file.h \
		dump_config.c dump_config.h \
		input_map.c input_map.h \
		transmit.c transmit.h

irrecord_LDADD = libhw_module.a @hw_module_libs@ @receive@
irrecord_DEPENDENCIES = libhw_module.a @receive@

## maintainer mode stuff
EXTRA_PROGRAMS = lircd.simsend lircd.simrec slinke
noinst_PROGRAMS = @maintmode_daemons_extra@
lircd_simsend_SOURCES = lircd.c ir_remote.c config_file.c \
		lircd.h ir_remote.h ir_remote_types.h config_file.h \
		input_map.c input_map.h \
		hw-types.c hw-types.h hardware.h \
		hw_default.c hw_default.h \
		receive.c receive.h \
		release.c release.h \
		transmit.c transmit.h
lircd_simsend_CFLAGS = -DSIM_SEND
lircd_simrec_SOURCES = lircd.c ir_remote.c config_file.c \
		lircd.h ir_remote.h ir_remote_types.h config_file.h \
		input_map.c input_map.h \
		hw-types.c hw-types.h hardware.h \
		hw_default.c hw_default.h \
		receive.c receive.h \
		release.c release.h \
		transmit.c transmit.h
lircd_simrec_CFLAGS = -DSIM_REC

slinke_SOURCES = slinke.c slinke.h config_file.c config_file.h \
		ir_remote.c ir_remote.h ir_remote_types.h \
		dump_config.c dump_config.h \
		release.c release.h \
		transmit.c transmit.h


if SANDBOXED
else
if USE_FIFOS
install-exec-local: mkfifo
uninstall-local: rmfifo
else
install-exec-local: 
uninstall-local: 
endif
endif

install-data-local:
	test "$(lircd_conf)" = ""  || test -e $(DESTDIR)$(sysconfdir)/$(PACKAGE)/lircd.conf  || ($(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lirc && $(INSTALL_DATA) $(top_srcdir)/remotes/$(lircd_conf) $(DESTDIR)$(sysconfdir)/$(PACKAGE)/lircd.conf)
	test "$(lircmd_conf)" = "" || test -e $(DESTDIR)$(sysconfdir)/$(PACKAGE)/lircmd.conf || ($(mkinstalldirs) $(DESTDIR)$(sysconfdir)/lirc && $(INSTALL_DATA) $(top_srcdir)/remotes/$(lircmd_conf) $(DESTDIR)$(sysconfdir)/$(PACKAGE)/lircmd.conf)

mkdev:
	test -e $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd || ($(mkinstalldirs) $(DESTDIR)$(varrundir)/lirc && @mknod@ $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd p)
	test -e $(DESTDIR)$(varrundir)/$(PACKAGE)/lircm || ($(mkinstalldirs) $(DESTDIR)$(varrundir)/lirc && @mknod@ $(DESTDIR)$(varrundir)/$(PACKAGE)/lircm p)

mkfifo:
	test -e $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd || ($(mkinstalldirs) $(DESTDIR)$(varrundir)/lirc && @mkfifo@ $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd)
	test -e $(DESTDIR)$(varrundir)/$(PACKAGE)/lircm || ($(mkinstalldirs) $(DESTDIR)$(varrundir)/lirc && @mkfifo@ $(DESTDIR)$(varrundir)/$(PACKAGE)/lircm)

rmdev:
rmfifo:
	-$(RM) $(DESTDIR)$(varrundir)/$(PACKAGE)/lircd
	-$(RM) $(DESTDIR)$(varrundir)/$(PACKAGE)/lircm

input_map.inc:
	$(srcdir)/input_map.sh >$@

DISTCLEANFILES = input_map.inc
EXTRA_DIST = input_map.inc input_map.sh

CLEANFILES = *~
