# *   Copyright (C) 1998-2014, International Business Machines
# *   Corporation and others.  All Rights Reserved.
UNIT_CLDR_VERSION = %version%
#
# A list of txt's to build
# The downstream packager may not need this file at all if their package is not
# constrained by
# the size (and/or their target OS already has ICU with the full locale data.)
#
# Listed below are locale data files necessary for 40 + 1 + 8 languages Chrome
# is localized to.
#
# Aliases which do not have a corresponding xx.xml file (see icu-config.xml &
# build.xml)
UNIT_SYNTHETIC_ALIAS =

# All aliases (to not be included under 'installed'), but not including root.
UNIT_ALIAS_SOURCE = $(UNIT_SYNTHETIC_ALIAS)

# Ordinary resources
UNIT_SOURCE =\
 en.txt en_001.txt\
 fr.txt
