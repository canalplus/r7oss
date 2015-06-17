#!/usr/bin/env perl
# vim:syntax=perl
use strict;
use warnings;
use File::Basename qw(dirname);
use Getopt::Long;
use Cwd qw(abs_path);
BEGIN {
    unshift(@INC, abs_path(dirname($0)));
}

use Board;
my $boardip;
my $f_rates = 0;
my $ENTRY_STAGE = "E";
my $EXIT_STAGE = "PM";
# Stages to monitor in the pipe
my @STAGES = ();
my $EXCLUSIVE = "#EXCLUSIVE";
# Graph size
my $w = 1920;
my $h = 1080;

my $filter;
my $ww = "120s";
my $v_in = "[03]";
my $v_out = "[14]";
my $title = "Streaming Engine pipeline";
my $with = "linespoints ps 0.2";
my $linreg;
my $f_update_pp = 1;
my $f_frc;

my $unit;
my $f_kernel_timestamps = 1;

# Display on TV ?
my $f_tv = 0;
my $pp = "parseNplot.pl";

use File::Basename;
my $ppdir = dirname(`which $pp`);


# Which streams ?
my $f_audio = 1;
my $f_video = 1;

my $f_injection = 1;
my $f_frameindex = 0;
my $f_pts = 0;

my $f_buffering = 1;
my $f_avsync = 1;
my $f_clear = 0;
my $f_mdtp = 0; # Only on Orly
my $f_live = 1;
my $video_sync_graph = "#Graph Sync";
my $audio_sync_graph = "#Graph Sync";
my $mdtp_sync_graph;
my $DEFAULT_FILTER = "(Stream 0x\\S+ - \\d -).* \\#(\\d+).*";
my $tail_cmd;
my $type;
my $graph;
my $file;
my $f_batch = 0;
my $f_smooth = 0;
my $f_single = 0;
my @flags =();
my @filters = ();
my $f_flags = 1;
my @events =();
my $f_events = 1;
sub getTailCmd {
    my $boardip = shift;
    return "ssh root\@$boardip tail -n 1000000 -F /var/log/kern.log" ;
}
sub help {
    print "\n Usage : \n\t$0 [--help][--tv][--state <k>]+ [-w <width>][-h <height>][--filter <expr>][--ww <window_width>][--title <title>][--cmd <input_cmd>][--boardip <IP>][--(no)audio][--(no)video]\n\n";
    print "\n Purpose : \n\n $0 can be used to monitor avsync and buffering in Streaming Engine pipeline\n";
    print " It will activate relevant traces for this purpose and use an ssh connection to a running target and will read traces from /var/log/kern.log\n";
    print " Please note that it makes the assumption that debugfs is mapped in /sys/kernel/debug\n";
    print " If this is not the case, please use this location instead, or manually activate needed traces (this will not alter the behaviour of this monitor)\n\n";
    print " Options :\n\n";
    print "  --help            : This help\n";
    print "  --tv              : Display on remote system\n";
    print "  --(no)live        : Live mode (or not)\n";
    print "  --smooth          : linear average on a sliding window\n";
    print "  [--stage <s>]+    : Stage in the pipeline to consider (can specify several)\n";
    print "  --entry <s>       : Entry stage in the pipeline, default : E\n";
    print "  --exit <s>        : Exit stage in the pipeline, default : PM\n";
    print "  -h <height>       : height of the graph in pixels, defaults to $h\n";
    print "  -w <width>        : width of the graph in pixels, defaults to $w\n";
    print "  --filter <expr>   : Filtering expression, defaults to \"$DEFAULT_FILTER\"\n";
    print "  --ww <width>      : sliding window width (defaults to \"$ww\")\n";
    print "  --title <title>   : Title of the graph (defaults to \"$title\")\n";
    print "  --boardip <IP>    : Board IP\n";
    print "  --(no)audio       : Audio (or not) : default is ON\n";
    print "  --(no)video       : Video (or not) : default is ON\n";
    print "  --(no)avsync      : AVSync graph (or not) : default is ON\n";
    print "  --(no)buffering   : Buffering graph (or not) : default is ON\n";
    print "  --(no)frameindex  : Buffering graph displays frame index instead of PTS\n";
    print "  --cmd <cmd>       : command used to retrieve data\n";
    print "  --file <file>     : overrides command mode, reads file instead\n";
    print "  --clear           : empty kern.log before start\n";
    print "  -k                : use kernel timestamps\n";
    print "  --batch           : do not open graph\n";
    print "  --type <type>     : use specified output file type (among png, jpg, svg)\n";
    print "  --graph <graph>   : set output file radix to <graph>\n";
    print "  --flag <expr>     : Adds selected flag\n";
    print "  --with <expr>     : Buffering graph using <with> gnuplot syntax\n";
    print "  --single          : output on single graph\n";
    print "  --noflags         : no flags on graph\n";
    print "  --noevents        : no events on graph\n";
    print "  --(no)frc         : events and flags regarding FRC\n";
    print "     \$> $tail_cmd\n\n";
    exit(0);

}

    GetOptions (
        'help!'   => \&help,
        'tv!'     => \$f_tv,
        'state|stage=s' => \@STAGES,
        'entry=s' => \$ENTRY_STAGE,
        'w=i'     => \$w,
        'h=i'     => \$h,
        'filter=s'=> \$filter,
        'ww=s'    => \$ww,
        'title=s' => \$title,
        'cmd=s'   => \$tail_cmd,
        'boardip=s'=> \$boardip,
        'audio!'  => \$f_audio,
        'video!'  => \$f_video,
        'type=s'   => \$type,
        'graph=s'   => \$graph,
        'buffering!'   => \$f_buffering,
        'frameindex|fi!'   => \$f_frameindex,
        'pts!'   => \$f_pts,
        'avsync!'   => \$f_avsync,
        'linreg=s' => \$linreg,
        'audio-sync-graph|asg=s' => \$audio_sync_graph,
        'video-sync-graph|vsg=s' => \$video_sync_graph,
        'mdtp-sync-graph|msg=s' => \$mdtp_sync_graph,
        'clear!'   => \$f_clear,
        'file|f=s' => \$file,
        'live!' => \$f_live,
        'kernel_timestamps|kern|k!' => \$f_kernel_timestamps,
        'batch!' => \$f_batch,
        'flag=s' => \@flags,
        'flags!' => \$f_flags,
        'filter=s' => \@filters,
        'event=s' => \@events,
        'events!' => \$f_events,
        'frc!'    => \$f_frc,
        'with=s' => \$with,
        'single!'=> \$f_single,
        'old!'=> sub{$EXCLUSIVE="";},
        'multiple'=> sub {$f_single=0;},
        'update!' => \$f_update_pp,

    );

# Silently checkout parseNplot latest version
if ($f_update_pp) {
    use Cwd;
    my $initialcwd = getcwd;
    my $output = `cd $ppdir && git fetch --all && git checkout origin/master `;
}


if ($f_frameindex) {
    $unit = "#UNIT SE Frame index";
} else {
    $unit = "#UNIT buffering in seconds when reaching given stage";
}


if (!defined $mdtp_sync_graph) {
    $mdtp_sync_graph = $video_sync_graph;
}
if ($f_smooth) {
    $linreg = "#SLIDING 1s" if !defined $linreg;
} else {
    $linreg = "";
}
if ($f_tv) {
    $pp = "ppdisp.pl";
}
my $pp_command = "$pp --title \"$title\"";
if (!defined $file) {
    if (!defined $boardip) {
        $boardip = Board::check_env();
    }
    if (!defined $boardip) {
        die "Exiting...";
    }
    die "Target $boardip is not responding" if !Board::board_is_alive($boardip);

    print "Target $boardip is alive. Let's proceed !\n";


    my $uname = Board::get_output($boardip, "uname -a");
    if ($uname =~ /h416/) {
        print "Running on Orly, monitoring MDTP clock\n";
        $f_mdtp = 1;
    }

    Board::ssh($boardip, "echo 5 > /sys/kernel/debug/havana/trace_activation/api");
    if ($f_buffering) {
        Board::ssh($boardip, "echo 5 > /sys/kernel/debug/havana/trace_activation/player");
    }
    else {
        Board::ssh($boardip, "echo -1 > /sys/kernel/debug/havana/trace_activation/player");
    }
    if ($f_avsync) {
        Board::ssh($boardip, "echo 5 > /sys/kernel/debug/havana/trace_activation/avsync");
    }
    else {
        Board::ssh($boardip, "echo -1 > /sys/kernel/debug/havana/trace_activation/avsync");
    }
    if ($f_clear) {
        Board::ssh($boardip, "> /var/log/kern.log");
    }
    if (!defined $tail_cmd) {
        $tail_cmd = getTailCmd($boardip);
    }


    $pp_command .= "\\\n --cmd \"$tail_cmd\" ";

}
else {
    $pp_command .= "\\\n $file";
}
if (!defined $filter) {
    $filter = $DEFAULT_FILTER;
}

sub uniq {
    my %seen = ();
    my @r = ();
    foreach my $a (@_) {
        unless ($seen{$a}) {
            push @r, $a;
            $seen{$a} = 1;
        }
    }
    return @r;
}
if ( scalar(@STAGES) == 0 ) {
    if ($f_single) {
        @STAGES= ($ENTRY_STAGE,"Discard","$EXIT_STAGE");

    }else {
        @STAGES= ($ENTRY_STAGE,"Discard","CtP","PtD","DtM","$EXIT_STAGE");
    }
    if (!$f_pts) {
        push @STAGES,"EXP";
    }
}

@STAGES = uniq(@STAGES);
my $slope = "";
my $xrange = "";
my $yrange = "--autoscale";
if ($f_rates) {
    $yrange = "--yrange 0:80";
    $slope = "#SLOPE 5s";
}

my $live_str;

if ($f_live) {
    $live_str = "--live";
} else {
    $live_str = "";
}
my $batch_str;

if ($f_batch) {
    $batch_str = "--batch";
} else {
    $batch_str = "";
}
$pp_command .= "\\\n --notswarn --ww $ww --absolute $live_str $batch_str  --nostatus ";

if (defined $type) {
    $pp_command .= "\\\n --type $type";
}

if (defined $graph) {
    $pp_command .= "\\\n --graph $graph";
}
if (!$f_kernel_timestamps) {
    if ($f_buffering) {

        $pp_command .= ' --timestamp "\s+$EXIT_STAGE=(\d+)" ';
    } elsif ($f_avsync) {
        $pp_command .= ' --timestamp "ActualTime = (\d+)" ';
    }
}
$pp_command .= "\\\n --filtersep '#' ";
$pp_command .= "\\\n --compact\\\n --width $w\\\n --height $h ";
$pp_command .= "\\\n $xrange ";
#$pp_command .= "\\\n $yrange ";

my $audio_buffering_graph;
my $video_buffering_graph;
my $show_event_only_graph = 0;
my $player_traces;


my $divider;
my $time_unit;
if ($f_single) {
    $divider = "CALC %v/1000000#MIN -10#MAX 10#";
    $graph = $mdtp_sync_graph = $video_sync_graph = $audio_sync_graph = "";
    $time_unit = "seconds";
}
else {
    $divider = "MIN -10000000#MAX 10000000#";
    $time_unit = "microseconds";

}


if ($f_avsync) {
    if ($video_sync_graph and $audio_sync_graph) {
        if ($f_video) {
            $graph = "$video_sync_graph";
        }
        if ($f_audio) {
            $graph = "$audio_sync_graph";
        }
    }

}
else {
    if ($f_buffering and !$f_single) {
        $show_event_only_graph = 1;
        $graph = "#Graph Events";
        $player_traces = "\" Player#WITH dots#AS Player API calls#SILENT$graph\" ";
    }
}

if ($f_buffering) {
    my $amongstreams = "";

    if ($f_audio) {
        $audio_buffering_graph = "#Graph Buffering of stream - 1 -";
        $amongstreams .= "- 1 -,";
    }
    if ($f_video) {
        $video_buffering_graph = "#Graph Buffering of stream - 2 -";
        $amongstreams .= "- 2 -,";
    }

    if ($f_pts or $f_frameindex) {
        $pp_command .= "'".'(Stream \S+\s+- \d -).*';
        my ($what,$unit,$calc);
        if ($f_pts) {
            $pp_command .= '.*PTS=(\d+).*';
            $what = "PTS";
            $unit = "$what back in seconds";
            $calc = '#CALC if (%v != 0) {%v/90000;} else {undef;}';
        }else {
            $pp_command .= '\\#(\d+)';
            $what = "frame counter";
            $unit = "Frames";
        }
        my $common_props = "#VALUE$calc#Y2#WITH $with#UNIT ";
        $common_props .= "$graph" if $f_single;
        $pp_command .= '\s+.*_STAGE_='.$common_props."#AMONG $amongstreams#AS $what at stage _STAGE_#TIMESTAMP _STAGE_=(\\d+)#REPLICATE _STAGE_=>";
        foreach my $stage (@STAGES) {
            $pp_command .= "$stage,";
        }
        $pp_command .= "' ";
        if ($f_pts and $f_video) {
            $pp_command .= "'DisplayCallback.*PTS=(\\d+)#VALUE#CALC if (%v != 0) {%v/90000;} else {undef;}#AS Display callback PTS#WITH points ps 0.5#Y2'";
        }
    } else {
        foreach my $stage (@STAGES) {
            # Filter itself
            $pp_command .= "\\\n'".$filter.".*$stage";
            $pp_command .= "#VALUE";
            $pp_command .= "#AMONG $amongstreams";
            $pp_command .= "$graph" if $f_single;
            $pp_command .= '#CALC ';

            $pp_command .= 'plot_on($f,"Buffering of $among");' if !$f_single;
            if ($stage eq $ENTRY_STAGE) {
            $pp_command .= 'if ($line =~ /\\#(\d+).*'.$ENTRY_STAGE.'=(\d+)/) {';
            $pp_command .=    '$SCRATCH{$among}{START}{$1} = $2;';
            $pp_command .=    '0;';
            } else {
            $pp_command .= 'if ($line =~ /\\#(\d+).*'.$stage.'=(\d+)/) {';
            $pp_command .=     'my $frameindex = $1; my $date = $2;';
            $pp_command .=     'if (defined($SCRATCH{$among}{START}{$frameindex})) {';
            $pp_command .=         '$tsus = $date;';
            $pp_command .=         '%v=($date-$SCRATCH{$among}{START}{$frameindex})/1000000;';
            $pp_command .=     '} else {';
            $pp_command .=         'undef;';
            $pp_command .=     '}';
            }
            $pp_command .= '} else {';
            $pp_command .=     'undef;';
            $pp_command .= '}';
            $pp_command .= $slope."#with ".$with.$linreg;
            $pp_command .= "#AS $stage";
            $pp_command .= "$unit ";
            if ($stage eq $ENTRY_STAGE) {
                $pp_command .= "' ";
            } else {
                $pp_command .= "$EXCLUSIVE' ";
            }
        }
    }
}


if ($f_avsync) {

    # Sync graph
    if ($f_video) {
        if ($f_mdtp) {
            $pp_command .=  "\\\n '.2-$v_in.*CurrentError = (\\S+)#VALUE#".$divider."with points ps 0.2 #AS Video MDTP Sync Error#unit $time_unit$mdtp_sync_graph#Y2$EXCLUSIVE' ";
        }
        $pp_command .=  "\\\n '.2-$v_out.*CurrentError = (\\S+)#VALUE#".$divider."with points ps 0.2 #AS Video Sync Error#unit $time_unit$video_sync_graph$EXCLUSIVE' ";
        $pp_command .=  "\\\n 'Percussive Adjustment Video#FLAG 100ms$video_sync_graph$EXCLUSIVE' ";
    }
    if ($f_audio) {
        $pp_command .=  "\\\n '.1-0.*CurrentError = (\\S+)#VALUE#".$divider."with points ps 0.2 #AS Audio Sync Error#unit $time_unit$audio_sync_graph$EXCLUSIVE' ";
        $pp_command .=  "\\\n 'Percussive Adjustment Audio#FLAG 100ms$audio_sync_graph$EXCLUSIVE' ";
    }
}

#Events

if ($f_events) {
    $pp_command .=  "\\\n $player_traces " if defined $graph and defined $player_traces;
    $pp_command .=  "\\\n '(A)djust(M)apping(B)ase.*- (\\S+)\$#EVENT$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n '(R)eset(T)ime(M)apping#EVENT$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n '(New Time Mapping).*(- \\d+ -).*(deltaNow=\\d+).*(StreamOffset=\\d+)#EVENT$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'SetPlaybackSpeed.*(Speed \\S+)#EVENT#COLOR green$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'starting (\\S+) playing (\\S+)#EVENT#COLOR magenta$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'Player::stm_se_(play_stream_drain).*(Stream 0x\\S+)#EVENT#COLOR red$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'PlayerStream_c..(Drain).*(Stream 0x\\S+.*)#EVENT#COLOR purple$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'Performing DrainLivePlayback#EVENT#COLOR magenta$graph$EXCLUSIVE' " if defined $graph;

    $pp_command .=  "\\\n '(SwitchStream).*(completed)#EVENT#COLOR plum$graph$EXCLUSIVE' " if defined $graph;

    foreach my $event (@events) {
        $pp_command .=  "\\\n '$event#EVENT$graph' ";

    }
}
if (!defined $f_frc and $f_events or $f_frc) {
    $pp_command .=  "\\\n '(Display Mode changed.*\\d\\d\\dx\\d\\d\\d.*)#EVENT#COLOR red$graph$EXCLUSIVE' " if defined $graph;
    $pp_command .=  "\\\n 'FRC.*pattern : (.*)#AMONG .*#FLAG 15ms$graph$EXCLUSIVE' " if defined $graph;
}

#Flags
if ($f_flags) {
    if ($f_injection) {
        $pp_command .=  "\\\n 'push_(inject)_data_se - >(Stream \\S+)#AMONG .*#FLAG 100ms$graph#COLOR magenta$EXCLUSIVE' ";
    }


    foreach my $flag (@flags) {
        $pp_command .=  "\\\n '$flag#FLAG$graph' ";

    }
}

if (scalar(@filters)) {

    foreach my $filter (@filters) {
        $pp_command .=  "\\\n '$filter' ";

    }

}

print "ParseNplot command : \n\n$pp_command\n";

exec($pp_command);

