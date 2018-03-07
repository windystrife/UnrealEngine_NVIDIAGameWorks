#!/usr/bin/env perl

# make sure we don't buffer postp, otherwise we won't be able to read the list of warnings and errors back in for failure emails
my $previous_handle = select(STDOUT);
$| = 1;
select(STDERR);
$| = 1;
select($previous_handle);

# check for a flag to filter out warning lines
my $no_warnings = grep(/--no-warnings/, @ARGV);

# keep track of whether to output messages
my $output_enabled = 1;

# read everything from stdin
while(<STDIN>)
{
	# remove timestamps added by gubp
	s/^\[[0-9:.]+\] //;

	# remove the mono prefix for recursive UAT calls on Mac
	s/^mono: ([A-Za-z0-9-]+: )/$1/;

	# remove the AutomationTool prefix for recursive UAT calls anywhere
	s/^AutomationTool: ([A-Za-z0-9-]+: )/$1/;
	
	# remove the gubp function or prefix
	s/^([A-Za-z0-9.-]+): // if !/^AutomationTool\\.AutomationException:/ && !/^error:/i && !/^warning:/i;

	# if it was the editor, also strip any additional timestamp
	if($1 && $1 =~ /^UE4Editor/)
	{
		s/^\[[0-9.:-]+\]\[\s*\d+\]//;
	}

	# skip over warnings if necessary
	if($no_warnings)
	{
		s/(?<![A-Za-z])[Ww]arning://g;
	}

	# look for a special marker to disable output
	$output_enabled-- if /<-- Suspend Log Parsing -->/i;

	# output the line
	if($output_enabled > 0)
	{
		print $_;
	}
	else
	{
		print "\n";
	}

	# look for a special marker to re-enable output
	$output_enabled++ if /<-- Resume Log Parsing -->/i;
}
