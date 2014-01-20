#!/usr/bin/perl
# parse lsb header and print as in OpenRC style.
# base code borrowed from update-rc.d of Debian

use Switch;
use File::Basename;

my $initdscript = shift;
my $name = basename($initdscript);
my %vsrv2u; # use ("+service", optional)
my %vsrv2n; # need

foreach $file (("/etc/insserv.conf", </etc/insserv.conf.d/*>)) {
    open(VSERV, "<$file") || die "error: unable to read $file";
    while (<VSERV>){
	next if (m/^\#.*/);
	chomp;
	if (m/^(\$\w+)\s*(\S?.*)$/i) {
	    my $vs=$1;
	    foreach $srv (split(/ /,$2)) {
		if ($srv =~ m/\+(\S?.*)/i) {
		    push(@{$vsrv2u{$vs}}, $1);
		} elsif ($srv =~ m/^(\$\w+)/i) {
		    @{$vsrv2u{$vs}} = (@{$vsrv2u{$vs}}, @{$vsrv2u{$1}});
		    @{$vsrv2n{$vs}} = (@{$vsrv2n{$vs}}, @{$vsrv2n{$1}});
		} elsif ($srv =~ m/^(\w+)/i) {
		    push(@{$vsrv2n{$vs}}, $1);
		}
	    }
	}
    }
}
# push(@{$vsrv2u{'\$all'}}, '*');

my %lsbinfo;
my $lsbheaders = "Provides|Required-Start|Required-Stop|Default-Start|Default-Stop|Short-Description|Description|Should-Start|Should-Stop|X-Start-Before|X-Start-After";
open(INIT, "<$initdscript") || die "error: unable to read $initdscript";
while (<INIT>) {
    chomp;
    $lsbinfo{'found'} = 1 if (m/^\#\#\# BEGIN INIT INFO\s*$/);
    last if (m/\#\#\# END INIT INFO\s*$/);
    if (m/^\# ($lsbheaders):\s*(\S?.*)$/i) {
	$lsbinfo{lc($1)} = $2;
    }
}
close(INIT);

my $des='';
my %dep;

sub l2of {
    my %args = @_;
    my $ret, $p = $args{prefix}, $sp = $args{sprefix};
    foreach $v (split(/ /, $args{items})) {
	if ($v =~ m/^\$/) {
	    @{$dep{$p}} = (@{$dep{$p}}, @{$vsrv2n{$v}});
	    @{$dep{$sp}} = (@{$dep{$sp}}, @{$vsrv2u{$v}}) if ($sp);
	} else {
	    push(@{$dep{$p}},$v)
	}
    }
}

# Check that all the required headers are present
if ($lsbinfo{found}) {
    foreach $key (keys %lsbinfo) {
	switch ($key) {
	    case "provides" {
		l2of(items=>$lsbinfo{$key}, prefix=>"provide");
	    }
	    case "required-start" {
		l2of(items=>$lsbinfo{$key}, prefix=>"need", sprefix=>"use");
	    }
	    case "required-stop" {}
	    case "default-start" {}
	    case "default-stop" {}
	    case "short-description" {
		$des .= 'description="'.$lsbinfo{$key}.'"'."\n";
	    }
	    case "description" {}
	    case "should-start" {
		l2of(items=>$lsbinfo{$key}, prefix=>"use", sprefix=>"use");
	    }
	    case "should-stop" {}
	    case "x-start-before" {
		l2of(items=>$lsbinfo{$key}, prefix=>"before", sprefix=>"before");
	    }
	    case "x-stop-after" {}
	}
    }
}

if (%dep) {
    $rst = "depend () {\n";
    foreach my $key ( keys %dep ) {
	$rst.="\t$key @{$dep{$key}}\n"	if (@{$dep{$key}})
    };
    $rst.="}\n"
};
$rst = $des . "\n" . $rst if ($des);

print $rst
