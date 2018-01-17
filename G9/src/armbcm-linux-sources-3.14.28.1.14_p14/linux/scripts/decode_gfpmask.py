#!/usr/bin/env python
"""
decode-gfpmask.py

Decodes the input GFP flags into their names.

Usage: decode-gfpmask.py <gfpflags in hex>
"""
# Copyright 2015 Broadcom Corporation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2, as
# published by the Free Software Foundation (the "GPL").
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# A copy of the GPL is available at
# http://www.broadcom.com/licenses/GPLv2.php or
# http://www.gnu.org/licenses/licenses.html
#
# Authors:
#     Gregory Fong <gregory@broadcom.com>


from __future__ import print_function
import re
import os.path
import sys


def gen_flags_dict():
    """Returns a dictionary, keys are GFP flag values and the values are the
    names"""

    linuxpath = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    d_gfp = {}
    with open(os.path.join(linuxpath, "include/linux/gfp.h")) as f:
        for line in f:
            line = line.rstrip()
            m = re.search("((?<=define )___GFP_\S+)\s+(0x\S+u)", line)
            if m:
                d_gfp[int(m.group(2)[:-1], 16)] = m.group(1)
    return d_gfp


def get_flag_names(d_gfp, flags):
    """Returns list of encoded 'flags' by looking them up in d_gfp"""
    i = 1
    flaglist = []
    while flags > 0:
        if flags & 1:
            flaglist.append(d_gfp[i])
        flags >>= 1
        i <<= 1
    return flaglist


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    if sys.argv[1] == "-h":
        print(__doc__)
        sys.exit(0)
    flags = int(sys.argv[1], 16)
    d_gfp = gen_flags_dict()
    print(" | ".join(get_flag_names(d_gfp, flags)))


if __name__ == "__main__":
    main()
