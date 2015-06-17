package Board;


my $sshopts = " -o PubkeyAuthentication=no -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no";

sub check_env {
    if ((!defined $ENV{BOARDIP} or !may_be_valid_ip($ENV{BOARDIP}))
            and (!defined $ENV{BIP} or !may_be_valid_ip($ENV{BIP}))
    ){

        my $fetched_bip = `ip neighbour | grep -v 254 | grep -e "REACHABLE\\|STALE" | cut -f 1 -d \\ `;
        chomp $fetched_bip;
        if (board_is_alive($fetched_bip)) {
            $ENV{BIP} = $ENV{BOARDIP} = $ENV{TARGETIP} = $fetched_bip;
            print "Retrieved Board ip : $ENV{BIP}. export BOARDIP or BIP to override\n";
        }
        else {

            print "Please define environment variable BOARDIP or BIP with the IP adress of the target\n";
            return 1;
        }
    }

    if (may_be_valid_ip($ENV{BIP}) and may_be_valid_ip($ENV{BOARDIP}) and $ENV{BOARDIP} ne $ENV{BIP}) {
        print "BIP=$ENV{BIP} BOARDIP=$ENV{BOARDIP}, BIP wins ! using $ENV{BIP}\n";
    }

    if (defined $ENV{BIP} and may_be_valid_ip($ENV{BIP})){
        print "Using BIP=$ENV{BIP}\n";
        return $ENV{BIP};
    }
    if (defined $ENV{BOARDIP} and may_be_valid_ip($ENV{BOARDIP})) {
        print "Using BOARDIP=$ENV{BOARDIP}\n";
        return $ENV{BOARDIP} ;
    }
}

sub may_be_valid_ip {
    my $ip = shift;
    return 0 if !defined $ip;
    return 0 if $ip =~ m/^\s*$/;
    return 1;
}

sub board_is_alive {
    my $boardip = shift;

    if (`ssh $sshopts root\@$boardip uname` =~ /Linux/i) {
        return 1;
    }
    return 0;
}
sub get_output {
    my $boardip = shift;
    my $arg = shift;
    my $cmd = "ssh $sshopts root\@$boardip \" $arg \"";
    print "Executing system command : $cmd\n";
    return `$cmd 2>&1`;
}
sub ssh {
    my $boardip = shift;
    my $arg = shift;
    my $cmd = "ssh $sshopts root\@$boardip \" $arg \"";
    print "Executing system command : $cmd\n";
    return system($cmd);
}

1;
