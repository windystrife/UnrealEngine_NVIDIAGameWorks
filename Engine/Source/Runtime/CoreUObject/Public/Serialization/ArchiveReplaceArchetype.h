// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ReloadObjectArc.h"

/*----------------------------------------------------------------------------
	FArchiveReplaceArchetype.
----------------------------------------------------------------------------*/
/**
 * This specialized version of the FReloadObjectArc is used when changing the archetype for a fully initialized object.  It handles saving and restoring
 * the values which have been changed in the instance, as well as remapping archetypes for subobjects to the corresponding subobject in the new archetype.
 * If a corresponding subobject cannot be found, the subobject's archetype is reset to the CDO for that subobject.
 */
class FArchiveReplaceArchetype : public FReloadObjectArc
{
public:
	FArchiveReplaceArchetype();

	/**
	 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
	 * is in when a loading error occurs.
	 *
	 * This is overridden for the specific Archive Types
	 **/
	virtual FString GetArchiveName() const { return TEXT("FArchiveReplaceArchetype"); }
};

