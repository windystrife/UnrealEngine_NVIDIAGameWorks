#!/usr/bin/perl

open(FSTAT, "p4 fstat -T \"depotFile, headChange, headAction, clientFile\" //depot/UE4/Engine/Documentation/Source/...udn |")    || die "can't run p4 fstat: $!";

$depotfile = "unknown";

while (<FSTAT>)
{
	chop;

	if( /depotFile\ (.*)/ )
	{
		$depotfile = $1;
	}

	if( /headChange\ ([0-9]+)/ )
	{
		$changelist = $1;

		if(	$depotfile =~ /(.*)\.INT\.udn$/ )
		{
			$INT{$1} = $changelist;
		}
		elsif(	$depotfile =~ /(.*)\.KOR\.udn$/ )
		{
			$KOR{$1} = $changelist;
		}
	}

	if( /headAction\ (.*)/ )
	{
		if( $1 eq "delete" || $1 eq "move/delete" )
		{
			if(	$depotfile =~ /(.*)\.INT\.udn$/ )
			{
				$DELETED_INT{$1} = $changelist;
			}
			elsif(	$depotfile =~ /(.*)\.KOR\.udn$/ )
			{
				$DELETED_KOR{$1} = $changelist;
			}
		}
	}

	if( /clientFile\ (.*)/ )
	{
		$clientfile = $1;
		if(	$depotfile =~ /(.*)\.INT\.udn$/ )
		{
			$filebase = $1;
			$INT_CLIENT{$filebase} = $clientfile;
			$korfile = $clientfile;
			$korfile =~ s/INT\.udn$/KOR\.udn/;
			$KOR_CLIENT{$filebase} = $korfile;
		}
	}
}
close(FSTAT)                      || die "can't close p4 fstat: $!";  

#remove deleted KOR and INT files from maps
foreach $file (sort (keys(%DELETED_INT)))
{
	delete $INT{$file};
}

foreach $file (sort (keys(%DELETED_KOR)))
{
	delete $KOR{$file};
}

foreach $file (sort (keys(%INT)))
{
	if( exists($KOR{$file}) ) 
	{
		$src_ver = $KOR{$file};

		# check if the KOR file has a source version number specified in its text, if so use that instead
		open(KORFILE, $KOR_CLIENT{$file});
		while(<KORFILE>)
		{
			if( /^INTSourceChangelist\:([0-9]+)/ )
			{
				$src_ver = $1;
				break;
			}
		}
		close(KORFILE);
		
		if ( $src_ver < $INT{$file} )
		{
			print "OUTDATED,diffrev3 $file.INT.udn ".$KOR_CLIENT{$file}." ".$src_ver." ".$INT{$file}." ".$INT_CLIENT{$file}."\n";
		}
	}
}

foreach $file (sort (keys(%INT)))
{
	if( !exists($KOR{$file}) ) 
	{
		print "MISSING,makenew ".$INT_CLIENT{$file}." ".$KOR_CLIENT{$file}." ".$INT{$file}."\n";
	}
}

foreach $file (sort (keys(%DELETED_INT)))
{
	if( exists($KOR{$file}) && exists($DELETED_INT{$file}) )
	{
		print "DELETED,p4 delete ".$KOR_CLIENT{$file}."\n";
	}
}
