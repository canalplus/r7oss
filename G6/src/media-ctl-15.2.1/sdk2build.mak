ifeq ($(SDK2_SOURCE_MEDIA_CTL),)
$(error SDK2_SOURCE_MEDIA_CTL must be specified to build the media_ctl module)
endif
ENVIRONMENT+=SDK2_SOURCE_MEDIA_CTL

packages+=media_ctl

USER_SPACE_MEDIA_CTL:=$(USERSPACE_DIR)/media_ctl

ifeq ($(ARCH),armv7)
else ifeq ($(ARCH),armv7_uclibc)
else ifeq ($(ARCH),sh4)
else ifeq ($(ARCH),sh4_uclibc)
else
$(error Wrong arch: $(ARCH))
endif

media_ctl_autoreconf_in  = $(addprefix $(USER_SPACE_MEDIA_CTL)/,configure.in \
								Makefile.am \
								src/Makefile.am)
media_ctl_autoreconf_out = $(addprefix $(USER_SPACE_MEDIA_CTL)/,configure \
								Makefile.in \
								src/Makefile.in \
								config.h.in)

media_ctl_configure_in   = $(addprefix $(USER_SPACE_MEDIA_CTL)/,configure \
								Makefile.in \
								src/Makefile.in \
								config.h.in \
								libmediactl.pc.in  \
								libv4l2subdev.pc.in)
media_ctl_configure_out  = $(addprefix $(USER_SPACE_MEDIA_CTL)/,config.status \
								config.log \
								Makefile \
								src/Makefile \
								config.h \
								libtool \
								libmediactl.pc \
								libv4l2subdev.pc)

media_ctl_make_in	 = $(addprefix $(USER_SPACE_MEDIA_CTL)/,config.status \
								Makefile \
								src/Makefile \
								config.h \
								libtool)

media_ctl_sources	:= $(shell cd $(SDK2_SOURCE_MEDIA_CTL) && find -type f -and -not -path './.git/*' | sed 's,^./,,')
media_ctl_sources_in	 = $(addprefix $(SDK2_SOURCE_MEDIA_CTL)/,$(media_ctl_sources))
media_ctl_sources_out    = $(addprefix $(USER_SPACE_MEDIA_CTL)/,$(media_ctl_sources))

TARGET_DIR?=$(STM_BASE_PREFIX)/devkit/$(target_arch)/target
STM_HOST_TOOLS_PREFIX?=$(STM_BASE_PREFIX)/host

$(USER_SPACE_MEDIA_CTL):
	$(Q)mkdir -p $(USER_SPACE_MEDIA_CTL)

# link files from $(media_ctl_sources_in) to $(media_ctl_sources_out)
$(USER_SPACE_MEDIA_CTL)/%:$(SDK2_SOURCE_MEDIA_CTL)/%
	$(Q)mkdir -p `dirname $@`
	$(Q)ln -sf $< $@

$(USER_SPACE_MEDIA_CTL)/.autom4te.cfg: $(STM_HOST_TOOLS_PREFIX)/share/autoconf/autom4te.cfg | $(USER_SPACE_MEDIA_CTL)
	@echo "Generating autoconf wrapper in media_ctl package"
	$(Q)mkdir -p $(USER_SPACE_MEDIA_CTL)/m4
	$(Q)sed s,/opt/STM/STLinux-2.4/host,$(STM_HOST_TOOLS_PREFIX),g $(STM_HOST_TOOLS_PREFIX)/share/autoconf/autom4te.cfg > $(USER_SPACE_MEDIA_CTL)/.autom4te.cfg

$(media_ctl_autoreconf_out): $(media_ctl_autoreconf_in) $(USER_SPACE_MEDIA_CTL)/.autom4te.cfg $(SDK2_SOURCE_MEDIA_CTL)/sdk2build.mak | $(media_ctl_sources_out)
	@echo "Running autoreconf in media_ctl package"
	$(Q)cd $(USER_SPACE_MEDIA_CTL) && autoreconf --verbose --install --force

$(media_ctl_configure_out): $(media_ctl_configure_in) $(SDK2_SOURCE_MEDIA_CTL)/sdk2build.mak | $(media_ctl_sources_out)
	@echo "Running configure in media_ctl package"
	$(Q)cd $(USER_SPACE_MEDIA_CTL) && \
	./configure --build=x86_64-pc-linux-gnu \
		    --host=$(_stm_target_config) \
		    --prefix=/usr \
		    --exec-prefix=/usr \
		    --bindir=/usr/bin \
		    --sbindir=/usr/sbin \
		    --sysconfdir=/etc \
		    --datadir=/usr/share \
		    --includedir=/usr/include \
		    --libdir=/usr/lib \
		    --libexecdir=/usr/libexec \
		    --localstatedir=/var \
		    --sharedstatedir=/usr/share \
		    --mandir=/usr/share/man \
		    --infodir=/usr/share/info \
		    --with-sysroot=$(TARGET_DIR)
	$(Q)touch $(USER_SPACE_MEDIA_CTL)/config.h

# Build and install in a single rule.
media_ctl: $(media_ctl_make_in) | $(media_ctl_sources_out)
	$(Q)$(MAKE) $(J) -C $(USER_SPACE_MEDIA_CTL)
	$(Q)$(MAKE) $(J) -C $(USER_SPACE_MEDIA_CTL) DESTDIR=$(TARGET_DIR) install

.clean_media_ctl: | $(media_ctl_sources_out)
	-$(Q)test -f $(USER_SPACE_MEDIA_CTL)/Makefile && $(MAKE) $(J) -C $(USER_SPACE_MEDIA_CTL) DESTDIR=$(TARGET_DIR) uninstall

.media_ctl_help:
	@printf "\n MEDIA_CTL specific targets:\n"
	@printf "        media_ctl - Build just the MEDIA_CTL application libraries\n"

.distclean_media_ctl:
	- $(Q)rm -rf $(USER_SPACE_MEDIA_CTL)

.PHONY: media_ctl .clean_media_ctl .media_ctl_help .distclean_media_ctl

# Thiis component is built when a dependancy requires it. No need to add it to EXTRA_RULES.
CLEAN_RULES+=.clean_media_ctl
HELP_RULES+=.media_ctl_help
DISTCLEAN_RULES+=.distclean_media_ctl

