#!/usr/bin/perl

# written by Grzegorz Dabrowski <gdx@poczta.fm>

sub analyze
{
  $i=-1;
  $what = shift;
  print "Analizying $what\n";
  open(F, "/tmp/all.i");
  while (<F>)
  {
      if (/(service|daemon)\s+([^\s]+)\s+[{:]/) {
  	$i++;
  	$tab0[$i] = $2;
      }
      if (/^[\s]+$what\s+=\s+([^;]+)[;]{0,1}$/) {
  	$tab[$i] = "$1";
      }
  }
  close(F);

  open(DOT, ">$what.dot");
  print DOT "digraph initng_$what {\n";
  for ($j=0; $j < $i; $j++)
  {
      $prov{$tab0[$j]} = 1;
      @needs = split(/\s+/, $tab[$j]);
      foreach $el (@needs)
      {
          print DOT "\"$tab0[$j]\" -> \"$el\" \n";
	  $req{$el} = 1;
      }
      $tab[$j] = "";
      $tab0[$j] = "";
  }
  print DOT "}\n";
  close(DOT);
  
  foreach $key (keys %req)
  {
    if ($prov{$key} == 0)
    {
    	    print "Warning: $key is probably not defined!\n";
    }
  }
}

analyze "need";
analyze "use";
