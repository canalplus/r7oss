#!/usr/bin/env python
"""
analyze-oom.py

Gives a rudimentary analysis of an OOM oops log.  Only analyzes the first OOM
message seen in the provided input.

Usage: analyze-oom.py <oopslog>
       cat <oopslog> | analyze-oom.py
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
from decode_gfpmask import gen_flags_dict, get_flag_names
import fileinput
import re
import sys


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "-h":
        print(__doc__)
        sys.exit(0)

    d_gfp = gen_flags_dict()

    gfp_mask = None
    order = None
    free_kB = None
    min_kB = None
    free_cma_kB = None
    free_highmem_kB = None
    min_highmem_kB = None
    free_highmem_cma_kB = None
    lowmem_reserve = None

    req_input_vars = ['gfp_mask', 'order', 'free_kB', 'min_kB',
                      'lowmem_reserve']

    # TODO: use actual PAGE_SIZE
    PAGE_SIZE = 4096
    PAGE_SIZE_kB = PAGE_SIZE / 1024

    for line in fileinput.input():
        line = line.rstrip()
        if gfp_mask is None:
            m = re.search("((?<=oom-killer: gfp_mask=)0x\w+),", line)
            if (m):
                gfp_mask = int(m.group(1), 16)
        if order is None:
            m = re.search("(?<=oom-killer: ).*order=(\d+),", line)
            if (m):
                order = int(m.group(1))
        if free_kB is None:
            m = re.search("((?<=Normal free:)\d+)kB", line)
            if (m):
                free_kB = int(m.group(1))
        if min_kB is None:
            m = re.search("(?<=Normal ).*min:(\d+)kB", line)
            if (m):
                min_kB = int(m.group(1))
        if free_cma_kB is None:
            m = re.search("(?<=Normal free:).*((?<=free_cma:)\d+)kB", line)
            if (m):
                free_cma_kB = int(m.group(1))
        if free_highmem_kB is None:
            m = re.search("((?<=HighMem free:)\d+)kB", line)
            if (m):
                free_highmem_kB = int(m.group(1))
        if min_highmem_kB is None:
            m = re.search("(?<=HighMem ).*min:(\d+)kB", line)
            if (m):
                min_highmem_kB = int(m.group(1))
        if free_highmem_cma_kB is None:
            m = re.search("(?<=HighMem free:).*((?<=free_cma:)\d+)kB", line)
            if (m):
                free_highmem_cma_kB = int(m.group(1))
        if lowmem_reserve is None:
            m = re.search("(?<=lowmem_reserve\[\]: 0 )(\d+)", line)
            if (m):
                lowmem_reserve = int(m.group(1))

    if any([eval(x) is None for x in req_input_vars]):
        print("Error: Missing {} in input log".format(x))
        sys.exit(1)

    gfp_flag_names    = get_flag_names(d_gfp, gfp_mask)
    has_cma           = (free_cma_kB is not None) or \
                        (free_highmem_cma_kB is not None)
    highmem           = '___GFP_HIGHMEM' in gfp_flag_names
    lowmem_reserve_kB = lowmem_reserve * PAGE_SIZE_kB
    # assume page group by mobility enabled (see include/linux/gfp.h)
    unmovable         = not any(x in gfp_flag_names for x in
                                ['___GFP_MOVABLE', '___GFP_RECLAIMABLE'])

    print("gfp_flags: 0x{:x}".format(gfp_mask))
    print("Decoded gfp_flags: {}".format(" | ".join(gfp_flag_names)))

    my_free_kB_calc = str(free_kB)  # for building up calculation

    print("Free mem in ZONE_NORMAL: {:d} kB.".format(free_kB))

    if free_highmem_kB:
        print("Free mem in ZONE_HIGHMEM: {:d} kB.".format(free_highmem_kB))
        my_free_kB_calc += " + " + str(free_highmem_kB)

    if unmovable and has_cma:
        print("Migrate type is MIGRATE_UNMOVABLE, cannot use CMA pages")
        print("Portion of ZONE_NORMAL which is CMA: {:d} kB."
              .format(free_cma_kB))
        free_kB -= free_cma_kB
        my_free_kB_calc += " - " + str(free_cma_kB)
        if free_highmem_cma_kB:
            free_highmem_kB -= free_highmem_cma_kB
            my_free_kB_calc += " - " + str(free_highmem_cma_kB)
            print("Portion of ZONE_HIGHMEM which is CMA: {:d} kB."
                  .format(free_highmem_cma_kB))

    my_free_kB = free_kB + free_highmem_kB

    print("Total free: {} = {:d} kB.".format(my_free_kB_calc, my_free_kB))

    my_min_kB = min_kB
    my_min_kB_calc = str(min_kB)
    if highmem and min_highmem_kB:
        my_min_kB += min_highmem_kB
        my_min_kB_calc += " + " + str(min_highmem_kB)

    print("Minimum RAM allowed: {} = {} kB".format(my_min_kB_calc, my_min_kB))

    if highmem and lowmem_reserve_kB:
        print("__GFP_HIGHMEM was set, must also exclude lowmem_reserve: {} kB"
                .format(lowmem_reserve_kB))

    print()
    print("Did we have enough memory?")

    had_enough_memory_lhs = my_free_kB - (PAGE_SIZE_kB << order)
    calc_lhs = str(my_free_kB) + " - " + str(PAGE_SIZE_kB << order)
    calcstr_lhs = "my_free_kB - (PAGE_SIZE_kB << order)"
    had_enough_memory_rhs = my_min_kB
    calc_rhs = str(my_min_kB)
    calcstr_rhs = "my_min_kB"
    if highmem:
        had_enough_memory_rhs += lowmem_reserve_kB
        calc_rhs += " + " + str(lowmem_reserve_kB)
        calcstr_rhs += " + lowmem_reserve_kB"
    print("Test: {} > {}".format(calcstr_lhs, calcstr_rhs))
    print("      {} > {}".format(calc_lhs, calc_rhs))
    had_enough_memory = had_enough_memory_lhs > had_enough_memory_rhs

    if had_enough_memory:
        print("*Yes*, had enough memory at time of OOM.")
    else:
        print("*No*, not enough memory at time of OOM.")

    print()
    print("Possible causes of OOM:")
    if had_enough_memory:
        print("- Allocation order was {}.".format(order),
              "Check freelist for that order and higher in the Mem-info.")
        if (unmovable and has_cma):
            print("  Note that this allocation is NOT allowed to use CMA",
                  "pages (marked 'C').")
        if highmem and free_highmem_kB:
            # TODO: check whether this was caused by having enough memory
            # in the sum of highmem and lowmem but not in either one
            pass
        # TODO: more sophisticated analysis of allocation order
        # that looks at freelist.  Maybe something like
        # if > 90% of free pages are in order 0 and order 1 blocks
        #         and this is a higher-order allocation:
        #     Report that this might be a core MM bug causing excessive
        #     fragmentation, please report to STB Linux team
    else:
        print("- If this took some time to fail, could be a memory leak in a",
              "kernel module.")
        if (highmem):
            print("- lowmem_reserve might be too high. Try tweaking",
                  "/proc/sys/vm/lowmem_reserve_ratio.")
        if (unmovable and has_cma):
            print("- May not have enough lowmem outside of a CMA region.",
                  "You may need to decrease the size of a region in lowmem.")

if __name__ == "__main__":
    main()
