#!/usr/bin/perl
#   Description: script to extract USB IDS from drivers files and generate
#                an HAL .fdi file to report connected USB IR controls
#   Current Version: 0.5
#   Author: dloic (loic.dardant AT gmail DOT com)
#
#	0.5 : hardcode .fdi filename and search path
#	0.4 : add comment with the name of the driver
#	0.3 : cleanup a bit the code and add comment for each remote (when it's in the text)
#	0.2 : add the "grep" unctionnality for constants
#	0.1 : creation of the script
#	
use File::Find;
use strict;

################# MAIN #################

# path to search
my 	$path="../../drivers/";

# output file
my $output="20-ircontrol-lirc.fdi";

# contain for each vendor an array with the different products
my %vendor;

#parse param, find what we want in the path and generate the output
&param;
find(\&find_ub,$path);
&hal_file;

################# SUB METHOD #################
sub hal_file
{
	#generate the header
	open my $out, ">$output" || die "error $output : $!";
	print $out '<?xml version="1.0" encoding="ISO-8859-1"?> <!-- -*- SGML -*- -->'."\n";
	print $out '<deviceinfo version="0.2">'."\n";
        print $out '  <device>'."\n";
        print $out '  <match key="info.bus" string="usb_device">'."\n";
	
	#generate the file in alphabetical order (first for vendor, then for product)
	foreach my $vendorId (sort { lc $a cmp lc $b } keys  %vendor)
	{
		print $out "  <!-- vendor -->\n";
 		print $out "  <match key=\"usb_device.vendor_id\" int=\"".$vendorId."\">\n";
	
		foreach my $productId (sort { lc $a cmp lc $b } keys %{$vendor{$vendorId}})
		{
			print $out "    <!-- ".$vendor{$vendorId}{$productId}{"comment"}.", driver : ".$vendor{$vendorId}{$productId}{"driver"}." -->\n";
			print $out "    <match key=\"usb_device.product_id\" int=\"".$productId."\">\n";
   			print $out '      <merge key="info.category" type="string">input</merge>'."\n";
   			print $out '      <append key="info.capabilities" type="strlist">input</append>'."\n";
   			print $out '      <append key="info.capabilities" type="strlist">input.remote_control</append>'."\n";
  			print $out '    </match>'."\n";
		}
		print $out "  </match>\n";
	}
	#generate footer
	print $out "  </match>\n";
	print $out "  </device>\n";
	print $out "</deviceinfo>\n";
}

sub find_ub
{
	my $nameFile=$_;
	my $lastComment="";
	
	if(!($nameFile =~/\.c$/))
	{
		return;
	}
	if($nameFile =~/^\./)
	{
		return;
	}
	if($nameFile =~/\.mod\.c$/)
	{
		return;
	}
	open my $file,$nameFile or die "error open file $nameFile";
	while(my $line=<$file>)
	{
		#catch comment (should permit comment on the precedent or on the current line of USB_DEVICE declaration)
		if($line =~/\s*\/\*(.+)\*\/\s*$/)
		{
			$lastComment=trim($1);
		}

		if($line =~/^\s*\{\s*USB_DEVICE\((.+)\,(.+)\)\s*\}/) # for example : { USB_DEVICE(VENDOR_ATI1, 0x0002) }
		{		
			my $VendorID=trim($1);
			my $ProductID=trim($2);
			
			#special thing if the guy is declaring a #DEFINE
			if(!($VendorID=~/\dx(\d|\w)+/))
			{
				my $data = do { open my $fh, $nameFile or die "error open file $nameFile"; join '', <$fh> };
				if ($data =~ /(#define|#DEFINE)\s+$VendorID\s+(\dx(\d|\w)+)/)
				{
					$VendorID=$2;
				}
				else
				{
					die "In file $nameFile, for vendor $VendorID, can't find the declaration of the constant";
				}
			}
			if(!($ProductID=~/\dx(\d|\w)+/))
			{
				my $data = do { open my $fh, $nameFile or die "error open file $nameFile"; join '', <$fh> };
				if ($data =~ /(#define|#DEFINE)\s+$ProductID\s+(\dx(\d|\w)+)/)
				{
					$ProductID=$2;
				}
				else
				{
					die "In file $nameFile, for product $ProductID, can't find the declaration of the constant";
				}
			}
			
			# FIXME: To be optimized for looking in ../../daemons
			if(!($vendor{$VendorID}{$ProductID}{"comment"}))
			{
				$vendor{$VendorID}{$ProductID}{"comment"}=$lastComment;
			}
			if(!($vendor{$VendorID}{$ProductID}{"driver"}))
			{
				my $driver="";
				if($nameFile=~/lirc_(.+)\.c/){ $driver=$1 };
				$vendor{$VendorID}{$ProductID}{"driver"}=$driver;
			}
		}
	}
}

sub param
{
	#useful var
	my $errorMessage="Usage: gen-hal-fdi.pl pathToScan\n";

	#for parameters
	# if(scalar(@ARGV)< 1){ die $errorMessage; };
	# For the moment, we suppose to be in contrib/hal
}


sub trim {
    my($str) = shift =~ m!^\s*(.+?)\s*$!i;
    defined $str ? return $str : return '';
}
