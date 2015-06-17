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
# Perform automated modifications to doxygen output to improve its organisation
# and visual appeal.
#

from __future__ import print_function
import argparse
import fileinput
import sys

'''Promote all sections and sub-sections up a level.

This filter is used to lift all the module documentation to be
first class chapters rather than having each module presented as
a section within a single chapter.
'''
def promote(lines):
    promotions = (r'\chapter', r'\section', r'\subsection',
                  r'\subsubsection', r'\subsubsubsection')

    for line in lines:
        for i in range(len(promotions)-1):
            line = line.replace(promotions[i+1], promotions[i])
        print(line.rstrip())


'''Organise the document and exclude useless chapters.

Applies only to refman.tex .

This filter rearranges the main .tex file to impose a specific ordering
upon the included files. Whilst working on this file we also eliminate
some redundant chapters that only clutter up the output.
'''
def organise(lines):
    def match_line(line, needles):
        for i in needles:
            if i in line:
                return True
        return False

    delete_these = (r'\chapter{Main Page}',
                    r'\label{index}', \
                    r'{lost.and.found', r'{Lost.and.found',
                    r'{module', r'{Module',
                    r'\include{group__internal__interfaces}',
                    r'\include{group__legacy__interfaces}')

    delay_these = (r'{Application notes}', r'{application_notes}',
                   r'{Revision history}', r'{revision_history}',
                   r'{Deprecated interfaces}', r'{deprecated_interfaces}',
                   r'{Deprecated List}', r'{deprecated}')

    trigger_delay = (r'\addcontentsline{toc}',)

    delay_list = []

    # This is the main loop (everything before this is meta-data to
    # support this loop).
    for line in lines:
        if match_line(line, delete_these):
            continue

        if match_line(line, delay_these):
            delay_list.append(line)
            continue

        if match_line(line, trigger_delay):
            for dl in delay_list:
                print(dl.rstrip())
            delay_list = []

        print(line.rstrip())

    # Ensure the delayed output was properly issued.
    assert(0 == len(delay_list))


'''Tweak the default style sheet to enhance visual appeal.

Applies only to doxygen.sty .

This filter changes a few small details of the default doxygen stylesheet.
In particular we remove gridlines from tables (by painting them white)
and tweak the column sizes to reduce the number of symbols that are
spread across multiple lines.
'''
def stylize(lines):
    whitelines = '''\

% Conceal all gridlines from tables (by painting them white)
\\arrayrulecolor{white}
'''

    for line in lines:
        if r'\RequirePackage[table]{xcolor}' in line:
            print(line.rstrip())
            print(whitelines)
            continue

        print(line.rstrip())

def main():
    parser = argparse.ArgumentParser(description='Perform automated modifications to doxygen output files.')

    parser.add_argument('filenames', metavar='filename', nargs='+',
                        help='Files to be processed')

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-p', '--promote', dest='action', action='store_const', const=promote,
                        help='Promote all sections and sub-sections up a level')
    group.add_argument('-g', '--organize', dest='action', action='store_const', const=organise,
                        help='Tweak the default style sheet to enhance visual appeal.')
    group.add_argument('-s', '--stylize', dest='action', action='store_const', const=stylize,
                        help='Tweak the default style sheet to enhance visual appeal.')

    args = parser.parse_args()

    for fname in args.filenames:
        f = fileinput.input(fname, inplace=True)
        args.action(f)
        f.close()

if __name__ == '__main__':
    main()
