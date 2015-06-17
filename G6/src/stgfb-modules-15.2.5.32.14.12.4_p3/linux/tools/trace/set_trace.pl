#!/usr/bin/env perl

use strict;
use warnings;
use Getopt::Long;
use Tk;
use Tk::Tree;
use IPC::Open3;
use Env qw( DISPLAY_ENGINE TARGETIP HWADDR);

sub help_cmdline {
    my(@args) = @_;
    my $element;

    print "\nusage: set_trace.pl [-file|-f debug_file] [-ip|-i board_ip] [-noconnect|-nc] [-noset|-ns] [-verbose|-v] [-help|-h]\n";

    if ( (scalar(@args) == 0) )
    {
        # empty argument means print all
        @args = ("-file", "-ip", "-noconnect", "-noset", "-verbose", "-help");
    }

    foreach $element (@args)
    {
        if ($element eq "-file")
        {
            print "         -file      : provide the full pathname of vibe_debug.h file to use\n";
            print "                      if not provided, use DISPLAY_ENGINE environment variable if exists\n";
        }
        if ($element eq "-ip")
        {
            print "         -ip        : provide board IP address\n";
            print "                      if not provided, use TARGETIP and/or HWADDR environment variable if exists\n";
        }
        if ($element eq "-noconnect")
        {
            print "         -noconnect : do not connect to the board\n";
        }
        if ($element eq "-noset")
        {
            print "         -noset     : even if connected to the board, do not set the mask on exit\n";
        }
        if ($element eq "-verbose")
        {
            print "         -verbose   : print some informations\n";
        }
        if ($element eq "-help")
        {
            print "         -help      : print this help\n";
        }
    }
    print "\n";

    return 0;
}

my $nb_mask = 2; # script is normaly ready to increase $nb_mask up to 4

my $debug_file = undef;
my $board_ip = undef;
my $verbose = undef;
my $help = undef;
my $no_connection = undef;
my $no_set = undef;

unless( GetOptions(
            "file|f=s" => \$debug_file,
            "noconnect|nc" => \$no_connection,
            "ip|i=s" => \$board_ip,
            "noset|ns" => \$no_set,
            "verbose|v" => \$verbose,
            "help|h" => \$help,
        ) )
{
    print "Error in command line arguments\n" ;
    help_cmdline();
    exit( -1 );
}

if ( defined $help )
{
    help_cmdline();
    exit( 0 );
}

#--- Check if environment variables are present ----------------------------------------------------

my @help_cmdline_on = ();

if ( not defined $debug_file )
{
    if ( not defined $DISPLAY_ENGINE )
    {
        print "unknown location for trace mask definition file (vibe_debug.h)\n";
        push @help_cmdline_on, "-file";
    }
}

if ( not defined $no_connection )
{
    if ( not defined $board_ip and (not defined $TARGETIP or $TARGETIP =~ /dhcp/) and not defined $HWADDR)
    {
        print "unknown target ip address\n";
        push @help_cmdline_on, "-ip";
    }
}

if( @help_cmdline_on )
{
    help_cmdline( @help_cmdline_on );
    exit( -1 );
}

#--- Get debug file name ---------------------------------------------------------------------------

if ( not defined $debug_file )
{
    if ( defined $DISPLAY_ENGINE )
    {
        if ( -e "$DISPLAY_ENGINE/private/include/vibe_debug.h" )
        {
            $debug_file = "$DISPLAY_ENGINE/private/include/vibe_debug.h";
        }
    }
}
else
{
    if ( not -e $debug_file )
    {
        $debug_file = undef;
    }
}

if ( not defined $debug_file )
{
    print "unable to find debug file\n";
    help_cmdline( "-file" );
    exit( -1 );
}

if ( defined $verbose ) { print "debug file : $debug_file\n"; }

#--- Get vibe_trace_mask value if connection requested ---------------------------------------------

my $l;
my $i;
my $set_mask = 0;
my @trc;
for ( $i = 0; $i < ( $nb_mask * 32 ); $i++ )
{
    $trc[$i] = 0;
}

if ( not defined $no_connection )
{
    if ( defined $verbose ) { print "...trying connect to board\n"; }
    if ( defined $TARGETIP and $TARGETIP !~ /dhcp/ )
    {
        $board_ip = $TARGETIP;
        print "using TARGETIP=$TARGETIP\n" if $verbose;
    }
    if ( not defined $board_ip )
    {
        if ( defined $verbose ) { print "......trying get board ip\n"; }
        if ( defined $HWADDR and ( $HWADDR =~ /..:..:..:..:..:../ ))
        {
            if ( system( "arp -n | grep -i $HWADDR > /tmp/arp.txt" ) != 0 )
            {
                print "unable to do: arp -n | grep -i " . $HWADDR . " > /tmp/arp.txt\n";
                exit( -1 );
            }
            unless( open( f_in, "</tmp/arp.txt" ) )
            {
                print "open /tmp/arp.txt failed\n";
                exit( -1 );
            }
            $l = <f_in>;
            if ( not defined ($l) )
            {
                # If file is empty, $l is undefined so testing its content will generate a warning
                # that's why it is forced to null string.
                $l = "";
            }
            unless( close( f_in ) )
            {
                print "close /tmp/arp.txt failed\n";
            }
            if ( system( "rm -f /tmp/arp.txt" ) != 0 )
            {
                print "rm -f /tmp/arp.txt failed\n";
            }

            if ( $l =~ /(\d+\.\d+\.\d+\.\d+)\s+ether/ )
            {
                $board_ip = $1;
            }
            else
            {
                print "board ip not found with MAC address " . $HWADDR . "\n";
            }
        }
    }

    if ( defined $board_ip )
    {
        if ( defined $verbose ) { print "board ip : $board_ip\n"; }
        if ( system( "ssh-keygen -R $board_ip 2>/dev/null" ) != 0 )
        {
            print "ssh-keygen -R " . $board_ip . " 2>/dev/null failed\n";
        }
        my $cmd = 'ssh -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -l root '.$board_ip;
        open3( \*CHLD_IN, \*CHLD_OUT, \*CHLD_OUT, $cmd ) or die "open3() failed $!";
        # Reading 2 times CHLD_OUT for clearing ssh command answer
        my $r = <CHLD_OUT>;
        $r = <CHLD_OUT>;
        # Loading vibe_os for reading vibe_trace_mask
        print CHLD_IN "modprobe vibe_os\n";
        print CHLD_IN "cat /sys/kernel/debug/vibe_trace_mask\n";
        $r = <CHLD_OUT>;

        if ( defined( $r ) )
        {
            if ( $r =~ /\s*(\d+)\s*(\d+)/ )
            {
                $set_mask = 1;
                my @val;
                for ( $i = 0; $i < $nb_mask; $i++ ) { $val[$i] = 0; }
                my $i_val = 0;
                my $idx = 0;
                $val[0] = $1;
                $val[1] = $2;

                if ( $nb_mask == 3 )
                {
                    if ( $r =~ /\s*\d+\s*\d+\s*(\d+)/ )
                    {
                        $val[2] = $1;
                    }
                }

                if ( $nb_mask == 4 )
                {
                    if ( $r =~ /\s*\d+\s*\d+\s*\d+\s*(\d+)/ )
                    {
                        $val[3] = $1;
                    }
                }

                for ( $i = 0; $i < ( $nb_mask * 32 ); $i++ )
                {
                    if (( $i > 0 ) and (( $i % 32 ) == 0 ))
                    {
                        $i_val++;
                        $idx += 32;
                    }

                    if ( $val[$i_val] & ( 1 << ( $i - $idx )))
                    {
                        $trc[$i] = 1;
                    }
                }
            }
            else
            {
                print "unable to get /sys/kernel/debug/vibe_trace_mask value\n"
            }
        }
    }
}

#--- Get trace identifiers in debug file -----------------------------------------------------------

my $enum = 0;
my $trace;
my $family;
my $comment;
my $find;
my @traces;
open( f_in, "<$debug_file" ) or die( "open $debug_file failed" );

while ( defined( $l = <f_in> )) {
    chomp $l;

    if ( $enum == 0 )
    {
        if ( $l =~ /enum trc_id/ )
        {
            $enum = 1;
            undef $trace;
            undef $family;
            undef $comment;
        }
    }
    else
    {
        if ( $l =~ /}/ )
        {
            $enum = 0;
            last;
        }
        else
        {
            $find = 1;

            if ( $l =~ /^\s*(TRC_ID_.*)\s*,\s*\/\/\s*(.*) - (.*)\s*$/ )
            {
                $trace = $1;
                $family = $2;
                $comment = $3;

                if ( $family eq "" )
                {
                    print "\nERROR - line with incorrect family : $l\n\n";
                }
            }
            elsif ( $l =~ /^\s*(TRC_ID_.*)\s*,\s*\/\/\s*(.*)\s*$/ )
            {
                $trace = $1;
                $comment = $2;

                if ( $comment eq '' )
                {
                    undef $family;
                    undef $comment;
                }
            }
            elsif ( $l =~ /^\s*(TRC_ID_.*)\s*,/ )
            {
                $trace = $1;
                undef $family;
                undef $comment;
            }
            else
            {
                $find = 0;
            }

            if ( $find == 1 )
            {
                push( @traces, $trace );
                push( @traces, $family );
                push( @traces, $comment );
            }
        }
    }
}

close( f_in );

#--- Manage window to select trace identifiers -----------------------------------------------------

#  Create TopLevel
my $top = MainWindow->new( -title => 'VIBE traces' );

my $frame = $top->Frame->pack( -fill => 'both',
                               -expand => 1 );

my $tree = $frame->Scrolled( 'Tree',
                             -width => 100,
                             -height => 30,
                             -scrollbars => 'osoe' )->pack( -fill => 'both',
                                                            -expand => 1 );

my $last_family = undef;
my $no_family = undef;
my $i_trc = 0;
my @win;
my $i_win = 0;

for ( $i = 0; $i < $#traces; )
{
    $trace = $traces[$i++];
    $family = $traces[$i++];
    $comment = $traces[$i++];
    #print "trace: $trace, family: $family, comment: $comment\n";

    if ( defined $trace )
    {
        if ( defined $family )
        {
            if ( not defined $last_family or ( $family ne $last_family ))
            {
                $tree->add( $family,
                            -text => $family );
                $last_family = $family;
            }

            $win[$i_win] = $tree->Checkbutton( -text => $trace."\t\t".$comment,
                                               -anchor => 'w',
                                               -variable => \$trc[$i_trc] );
            $tree->add( $family.'.'.$trace,
                        -widget => $win[$i_win],
                        -itemtype => 'window' );
            $tree->hide( 'entry',
                         $family.'.'.$trace );
            $i_win++;
        }
        else
        {
            if ( not defined $no_family )
            {
                $no_family = 'NO FAMILY';
                $tree->add( $no_family,
                            -text => $no_family );
            }

            $win[$i_win] = $tree->Checkbutton( -text => $trace,
                                               -anchor => 'w',
                                               -variable => \$trc[$i_trc] );
            $tree->add( $no_family.'.'.$trace,
                        -widget => $win[$i_win],
                        -itemtype => 'window' );
            $tree->hide( 'entry',
                         $no_family.'.'.$trace );
            $i_win++;
        }
    }

    if ( $i_trc++ >= ( $nb_mask * 32 ))
    {
        print "NEED TO INCREASE VIBE_TRACE_MASK SIZE\n";
        exit( 1 );
    }
}

$tree->autosetmode();
MainLoop;

#--- Calculate new vibe_trace_mask value and optionaly set it --------------------------------------

my @val;
for ( $i = 0; $i < $nb_mask; $i++ ) { $val[$i] = 0; }
my $i_val = 0;
my $idx = 0;

for ( $i = 0; $i < $i_trc; $i++ )
{
    if (( $i > 0 ) and (( $i % 32 ) == 0 ))
    {
        $i_val++;
        $idx += 32;
    }

    $val[$i_val] += $trc[$i] << ( $i - $idx );
}

if ( not defined $no_set and ( $set_mask == 1 ))
{
    if ( $nb_mask == 2 ) { print CHLD_IN "echo $val[0] $val[1] > /sys/kernel/debug/vibe_trace_mask\n"; }
    elsif ( $nb_mask == 3 ) { print CHLD_IN "echo $val[0] $val[1] $val[2] > /sys/kernel/debug/vibe_trace_mask\n"; }
    elsif ( $nb_mask == 4 ) { print CHLD_IN "echo $val[0] $val[1] $val[2] $val[3] > /sys/kernel/debug/vibe_trace_mask\n"; }
}
else
{
    printf "\nCommand to apply:\n";
    if ( $nb_mask == 2 ) { printf "echo 0x%x 0x%x > /sys/kernel/debug/vibe_trace_mask\n", $val[0], $val[1]; }
    elsif ( $nb_mask == 3 ) { printf "echo 0x%x 0x%x 0x%x > /sys/kernel/debug/vibe_trace_mask\n", $val[0], $val[1], $val[2]; }
    elsif ( $nb_mask == 4 ) { printf "echo 0x%x 0x%x 0x%x 0x%x> /sys/kernel/debug/vibe_trace_mask\n", $val[0], $val[1], $val[2], $val[3]; }
}

exit( 0 );
