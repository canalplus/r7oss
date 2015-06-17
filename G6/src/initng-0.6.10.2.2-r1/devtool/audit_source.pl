#! /usr/bin/perl -w

# This script looks in the source tree to find source files that use
# memory functions, such as malloc(), realloc(), and so on. initng has
# its own functions which should be used instead.

use strict;
use English;

use File::Find;

sub wanted 
{ 
    # ignore dot-files
    if ($_ =~ /^\.[^.]/) {
        $File::Find::prune = 1;
        return;
    }
    # we only want "*.[ch]"
    return unless ($_ =~ /\.[ch]$/);
    #return if (-d $_);
    open(FILE, "< $_") || die;
    my $buf = <FILE>;
    if ($buf =~ /malloc\s*\(/) {
        # don't warn about ash files, because they aren't part of init
        unless (($_ eq "ash.c") or ($_ eq "libbb.h") or ($_ eq "xfuncs.c")) {
            print $File::Find::name, "\n";
        }
    } elsif ($buf =~ /[^_]realloc\s*\(/) {
        # don't warn about toolbox
        print $File::Find::name, "\n" unless ($_ eq "initng_toolbox.c");
    } elsif ($buf =~ /[^_]strdup/) {
        # don't warn about toolbox
        print $File::Find::name, "\n" unless ($_ eq "initng_toolbox.c");
    } elsif ($buf =~ /[^_]strndup/) {
        # don't warn about toolbox
        print $File::Find::name, "\n" unless ($_ eq "initng_toolbox.c");
    } elsif ($buf =~ /[^_]calloc\s*\(/) {
        # don't warn about toolbox, or ngc2 (exectable)
        unless (($_ eq "ngc2.c") or ($_ eq "initng_toolbox.c")) {
            print $File::Find::name, "\n";
        }
    }

    if ($buf =~ /[^a-z]printf\s*\(\s*"/) {
        # don't warn about ngc2 (executable), or a few plugins that 
        # are designed to output to the screen
        unless (($_ eq "ngc2.c") or ($_ eq "initng_colorprint_out.c") or 
                ($_ eq "initng_interactive.c")) {
            print "$File::Find::name has printf\n";
        }
    }
    close(FILE) || die;
}

undef $INPUT_RECORD_SEPARATOR;
find(\&wanted, ("../src", "../plugins"));


