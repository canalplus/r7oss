#!/usr/bin/env perl

use strict;
use warnings;

my ( $f_in_0, $f_in_1 );
my ( $lc0, $lc1 );
my $ti = 0;

sub get_line
{
    my ( $f, $tl ) = @_;

    $$tl = undef;

    if ( not defined $f ) {
        return( undef );
    }

    while ( defined( my $l = <$f> )) {
        chomp $l;

        if ( $l =~ /(\d+\.\d+)\s+\d+\s+\w\s+"vibe / ) {
            $$tl = $1;
            return( $l );
        }
        elsif (( $ti == 0 ) and ( $l =~ /^Time: (\d+\.\d+)/ )) {
            $ti = $1;
        }
    }

    return( undef );
}

if ( $#ARGV > 1 )
{
    print "error: number of parameters\n";
    print "usage: trc.pl <trace core 0> [<trace core 1>]\n";
    exit( 1 );
}

open( $f_in_0, "<$ARGV[0]" ) or die( "open $ARGV[0] failed" );

if ( $#ARGV == 1 ) {
    open( $f_in_1, "<$ARGV[1]" ) or undef $f_in_1;
} else {
    undef $f_in_1;
}

my $tlc0 = 0;
my $tlc1 = 0;
$lc0 = get_line( $f_in_0, \$tlc0 );
$lc1 = get_line( $f_in_1, \$tlc1 );
my $l;
my $c;
my $d = 0;
my $tc = 0;

do
{
    if ( defined( $lc0 ) and defined( $lc1 )) {
        if ( $tlc0 > $tlc1 ) {
            if ( $tc != 0 ) {
                $d = $tlc1 - $tc;
            }

            $c = 1;
            $l = $lc1;
            $tc = $tlc1;
        }
        elsif ( $tlc0 < $tlc1 ) {
            if ( $tc != 0 ) {
                $d = $tlc0 - $tc;
            }

            $c = 0;
            $l = $lc0;
            $tc = $tlc0;
        }
        else {
            if ( $tc != 0 ) {
                $d = $tlc1 - $tc;
            }

            # same time, we continue on same core
            $tc = $tlc1;
        }
    }
    elsif ( defined( $lc0 )) {
        if ( $tc != 0 ) {
            $d = $tlc0 - $tc;
        }

        $c = 0;
        $l = $lc0;
        $tc = $tlc0;
    }
    elsif ( defined( $lc1 )) {
        if ( $tc != 0 ) {
            $d = $tlc1 - $tc;
        }

        $c = 1;
        $l = $lc1;
        $tc = $tlc1;
    }
    else {
        $l = undef;
    }

    if ( defined( $l )) {
        if ( $l =~ /\d+\.\d+\s+\d+\s+\w\s+"vibe (.*)"/ ) {
            printf "%s: %f (%.6f) %s\n", $c, $tc - $ti, $d, $1;
        }

        if ( $c == 0 ) {
            $lc0 = get_line( $f_in_0, \$tlc0 );
        }
        else {
            $lc1 = get_line( $f_in_1, \$tlc1 );
        }
    }
}
while (( defined( $lc0 ) or defined( $lc1 )));

if ( defined $f_in_1 ) {
    close( $f_in_1 );
}

close( $f_in_0 );
exit( 0 );
