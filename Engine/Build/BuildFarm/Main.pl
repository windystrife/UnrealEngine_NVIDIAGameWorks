# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

use strict;
use warnings;
use Data::Dumper;
use File::Copy;
use File::Path;
use File::Find;
use List::Util ('min', 'max');
use ElectricCommander();
use JSON;
use URI::Escape;

use Commands;

execute_command(\@ARGV);
