# *   Copyright (C) 1998-2010, International Business Machines
# *   Corporation and others.  All Rights Reserved.
CURR_CLDR_VERSION = %version%
# A list of txt's to build
# The downstream packager may not need this file at all if their package is not
# constrained by
# the size (and/or their target OS already has ICU with the full locale data.)
#
# Listed below are locale data files necessary for 40 + 1 + 8 languages Chrome
# is localized to.
#
# In addition to them, 40+ "abridged" locale data files are listed. Chrome is
# localized to them, but
# uses a few categories of data in those locales for IDN handling and language
# name listing (in the UI).
# Aliases which do not have a corresponding xx.xml file (see icu-config.xml &
# build.xml)
CURR_SYNTHETIC_ALIAS =

# All aliases (to not be included under 'installed'), but not including root.
CURR_ALIAS_SOURCE = $(CURR_SYNTHETIC_ALIAS)

# Ordinary resources
CURR_SOURCE =\
 en.txt en_001.txt en_150.txt\
 fr.txt
