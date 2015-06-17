#!/usr/bin/env python2.7

#Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.
#
#This file is part of the Streaming Engine.
#
#Streaming Engine is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 as
#published by the Free Software Foundation.
#
#Streaming Engine is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#See the GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License along
#with Streaming Engine; see the file COPYING.  If not, write to the Free
#Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
#USA.
#
#The Streaming Engine Library may alternatively be licensed under a
#proprietary license from ST.

#
# Pre-filter doxygen input to filter out a couple of small issues that confuse
# its parser.
#

from __future__ import print_function
import fileinput

'''Identity __attribute__ declarations and remove them.

Prototypical form is: __attribute__((deprecated,const))

We return unmodified any line where __attribute__ is not found
or where the next non-whitespace character *after* __attribute__
is not an opening brace.

If we decide to modify a line we conceal all characters from the
__attribute__ to the first balanced closing brace.
'''
def attribute_filter(line):
    trigger = '__attribute__'
    offset = line.find(trigger)
    if offset == -1:
        return line

    ending = line[offset + len(trigger):].lstrip()
    if ending[0] != '(':
        return line

    line = line[:offset]
    ending = ending[1:]
    depth = 1
    for c in ending:
        if 0 == depth:
            line += c
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1

    return line

def main():
    for line in fileinput.input():
        line = attribute_filter(line)

        print(line.rstrip())

if __name__ == '__main__':
    main()
