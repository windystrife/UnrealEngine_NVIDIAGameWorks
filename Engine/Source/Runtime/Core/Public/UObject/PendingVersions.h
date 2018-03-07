// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PendingVersions.h: A list of engine versions which are awaiting insertion
	                   into EUnrealEngineObjectUE4Version.
	
	                   This file is not intended to be changed in Main (and is
	                   in fact locked there), but only used by branches to
	                   provide an engine version identifier to code against
	                   prior to a merge to Main.
	
	                   When the merge takes place, the identifier should be
	                   moved out of here and into the bottom of 
	                   EUnrealEngineObjectUE4Version as usual.
	
	                   All versions defined in this file should be given a
	                   value of 0xFFFFFFFF.
=============================================================================*/

// Example:
// enum { VER_UE4_SOMETHING_ADDED = 0xFFFFFFFF };
