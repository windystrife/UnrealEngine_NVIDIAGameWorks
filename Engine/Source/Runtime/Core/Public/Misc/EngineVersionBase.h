// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** Enum for the components of a version string. */
enum class EVersionComponent
{
	Major,					///< Major version increments introduce breaking API changes.
	Minor,					///< Minor version increments add additional functionality without breaking existing APIs.
	Patch,					///< Patch version increments fix existing functionality without changing the API.
	Changelist,				///< The pre-release field adds additional versioning through a series of comparable dotted strings or numbers.
	Branch,					///<
};


/** Components of a version string. */
enum class EVersionComparison
{
	Neither,
	First,
	Second,
};


/** Base class for the EngineVersion class. Holds basic version numbers. */
class CORE_API FEngineVersionBase
{
public:

	/** Empty constructor. Initializes the version to 0.0.0-0. */
	FEngineVersionBase();

	/** Constructs a version from the given components. */
	FEngineVersionBase(uint16 InMajor, uint16 InMinor, uint16 InPatch = 0, uint32 InChangelist = 0);

	/** Returns the changelist number corresponding to this version. */
	uint32 GetChangelist() const;	

	/** Returns the Major version number corresponding to this version. */
	FORCEINLINE uint16 GetMajor() const
	{
		return Major;
	}

	/** Returns the Minor version number corresponding to this version. */
	FORCEINLINE uint16 GetMinor() const
	{
		return Minor;
	}

	/** Returns the Patch version number corresponding to this version. */
	FORCEINLINE uint16 GetPatch() const
	{
		return Patch;
	}

	/** Checks if the changelist number represents licensee changelist number. */
	bool IsLicenseeVersion() const;

	/** Returns whether the current version is empty. */
	bool IsEmpty() const;

	/** Returns whether the engine version has a changelist component. */
	bool HasChangelist() const; 

	/** Returns the newest of two versions, and the component at which they differ */
	static EVersionComparison GetNewest(const FEngineVersionBase &First, const FEngineVersionBase &Second, EVersionComponent *OutComponent);

	/** Encodes a licensee changelist number (by setting the top bit) */
	static uint32 EncodeLicenseeChangelist(uint32 Changelist);

protected:

	/** Major version number. */
	uint16 Major;

	/** Minor version number. */
	uint16 Minor;

	/** Patch version number. */
	uint16 Patch;

	/** Changelist number. This is used to arbitrate when Major/Minor/Patch version numbers match. Use GetChangelist() instead of using this member directly. */
	uint32 Changelist;
};
