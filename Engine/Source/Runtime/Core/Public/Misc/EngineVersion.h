// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EngineVersionBase.h"
#include "Containers/UnrealString.h"

/** Utility functions. */
class CORE_API FEngineVersion : public FEngineVersionBase
{
public:

	/** Empty constructor. Initializes the version to 0.0.0-0. */
	FEngineVersion();

	/** Constructs a version from the given components. */
	FEngineVersion(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch);

	/** Sets the version to the given values. */
	void Set(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch);

	/** Clears the object. */
	void Empty();

	/** Checks compatibility with another version object. */
	bool IsCompatibleWith(const FEngineVersionBase &Other) const;

	/** Generates a version string */
	FString ToString(EVersionComponent LastComponent = EVersionComponent::Branch) const;

	/** Parses a version object from a string. Returns true on success. */
	static bool Parse(const FString &Text, FEngineVersion &OutVersion);

	/** Gets the current engine version */
	static const FEngineVersion& Current();

	/** Gets the earliest version which this engine maintains strict API and package compatibility with */
	static const FEngineVersion& CompatibleWith();

	/** Overrides the current changelist in the verison */
	static bool OverrideCurrentVersionChangelist(int32 NewChangelist, int32 NewCompatibleChangelist);

	/** Serialization function */
	friend CORE_API void operator<<(class FArchive &Ar, FEngineVersion &Version);

	/** Returns the branch name corresponding to this version. */
	const FString GetBranch() const
	{
		return Branch.Replace( TEXT( "+" ), TEXT( "/" ) );
	}

	const FString& GetBranchDescriptor() const;
		

private:

	/** Branch name. */
	FString Branch;

	/** Global instance of the current engine version. */
	static FEngineVersion CurrentVersion;

	/** Earliest version which this engine maintains strict API and package compatibility with */
	static FEngineVersion CompatibleWithVersion;
};

/** Version used for networking; the P4 changelist number. */
DEPRECATED( 4.13, "Please use FNetworkVersion::GetNetworkCompatibleChangelist intead." )
CORE_API extern int32 GEngineNetVersion;

/** Earliest engine build that is network compatible with this one. */
DEPRECATED( 4.13, "GEngineMinNetVersion is no longer a valid property." )
CORE_API extern const int32 GEngineMinNetVersion;

/** Base protocol version to negotiate in network play. */
DEPRECATED( 4.13, "GEngineNegotiationVersion is no longer a valid property." )
CORE_API extern const int32 GEngineNegotiationVersion;
