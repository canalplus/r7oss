#!/usr/bin/perl

$elf_file = shift;
$LoaderFn = shift;               # "STPTI_Loader_$LoaderID";
die "SYNTAX:  $0 <ElfFile>\n" unless defined($elf_file);

die "Cannot open $elf_file" unless -e $elf_file;



# First we look for which sections are LOADed.
open FH, "-|", "objdump -h $elf_file" or die "Cannot Open $elf_file";
$flag=0;
while(<FH>)
{
    $pgheader .= $_;
    
    if( $flag )
    {
        push @sections, $section if( /\bLOAD\b/ );              # add to list if LOAD flag found
        $flag = 0;
    }

    $section = $1, $flag=1 if( /\s*\d+\s\.(\S+)/ )              # found a matching line describing a section (next line will have section's FLAGS */
}
close FH;
# @sections now holds a list of all the section that will make up the loader (LOADable)


foreach $section (@sections)
{
    # dump out the section...
    open FH, "-|", "objdump -s -j .$section $elf_file" or die;

    undef $expected_next_addr;
    $section_size{$section} = 0;
    $section_allzeros{$section} = 1;
    $flag=0;
    while(<FH>)
    {
        # format is...  4390 8c854000 00000000 00000000 98430000  ..@..........C..
        # where 4390 is the address, followed by a space, and then 32bit BE data bytes, and there are two spaces before the ASCII equivalent
        if( $flag and m/^\s*([0-9a-fA-F]+)\s+(.*?)  /)
        {
            $addr = hex($1);
            $bytes_as_hex = $2;
            $bytes_as_hex =~ s/\s*//g;
            $string="\"";
            $string2="";
            $bytes_this_line=0;
            while($bytes_as_hex=~s/^(..)//g)
            {
                $byte = hex($1);
                $string.=sprintf( "\\x%02x", $byte );
                $character=sprintf( "%c", $byte );
                $character="." if ($byte<0x20) or ($byte>=0x7F);
                $string2.=$character;
                $section_size{$section}++;
                $section_allzeros{$section}=0 if($byte!=0);
                $bytes_this_line++;
            }
            $string.="\"";

            if( defined($expected_next_addr) )
            {
                die "discountinuity $addr != $expected_next_addr" if( $expected_next_addr != $addr )
            }
            else
            {
                $section_start{$section} = $addr;
            }

            $expected_next_addr = $addr + $bytes_this_line;
            $string2 = "/*".$string2."*/";
            $section_data{$section}.=sprintf("        %-70s %-25s \\\n", $string, $string2 );
        }

        # The dump of the section begins with "Contents of section"
        $flag =1 if( m/^Contents of section/ )
    }
    
    close FH;
}


# Create loader
while(<DATA>)
{
    if( m/(\s*)\/\*\s*Program Header Table\s*\*\//i )
    {
        $leading_spacing = $1;
        $pgheader=~s/^/$leading_spacing/mg;
        print "/*\n$pgheader*/\n\n";
    }
    elsif( m/(\s*)\/\*\s*elf_section_arrays\s*\*\//i )
    {
        # output the elf section arrays
        printf("%-104s \\\n", "#define $LoaderFn");
        printf("%-104s \\\n", "{");
        foreach $section (@sections)
        {
            if( $section_size{$section} > 0 and not $section_allzeros{$section} )
            {
                $string=sprintf("   { \".%s\", 0x%08x, %d,", $section, $section_start{$section}, $section_size{$section});
                printf("%-104s \\\n", $string);
                print $section_data{$section};
                printf("%-104s \\\n", "   },");
            }
        }
        printf("}\n");
    }
    else
    {
        print;
    }
}






__DATA__
/******************************************************************************
COPYRIGHT (C) STMicroelectronics 2008, 2009, 2010, 2011.  All rights reserved.
******************************************************************************/

/* Program Header Table */

/* elf_section_arrays */


