#
# Makefile for ALSA Echoaudio soundcard drivers
# Copyright (c) 2003 by Giuliano Pochini <pochini@shiny.it>
#

ifndef SND_TOPDIR
SND_TOPDIR=../..
endif

include $(SND_TOPDIR)/toplevel.config
include $(SND_TOPDIR)/Makefile.conf

clean-files := echoaudio.c darla20.c darla24.c echo3g.c gina20.c gina24.c \
		indigo.c indigodj.c indigoio.c layla20.c layla24.c mia.c mona.c

include $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/Makefile

include $(SND_TOPDIR)/Rules.make

echoaudio.c: echoaudio.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/echoaudio.c
darla20.c: darla20.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/darla20.c echoaudio.c
darla24.c: darla24.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/darla24.c echoaudio.c
echo3g.c: echo3g.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/echo3g.c echoaudio.c
gina20.c: gina20.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/gina20.c echoaudio.c
gina24.c: gina24.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/gina24.c echoaudio.c
indigo.c: indigo.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/indigo.c echoaudio.c
indigodj.c: indigodj.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/indigodj.c echoaudio.c
indigoio.c: indigoio.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/indigoio.c echoaudio.c
layla20.c: layla20.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/layla20.c echoaudio.c
layla24.c: layla24.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/layla24.c echoaudio.c
mia.c: mia.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/mia.c echoaudio.c
mona.c: mona.patch $(SND_TOPDIR)/alsa-kernel/pci/echoaudio/mona.c echoaudio.c
