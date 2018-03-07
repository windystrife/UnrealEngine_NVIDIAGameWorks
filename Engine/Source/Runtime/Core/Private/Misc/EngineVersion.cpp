// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/EngineVersion.h"
#include "Misc/Guid.h"
#include "Serialization/CustomVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/ReleaseObjectVersion.h"

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#define VXGI_BRANCH_NAME BRANCH_NAME "+VXGI-" VXGI_VERSION_STRING
#endif
// NVCHANGE_END: Add VXGI

/** Version numbers for networking - DEPRECATED!!!! Use FNetworkVersion::GetNetworkCompatibleChangelist instead!!! */
int32 GEngineNetVersion = ENGINE_NET_VERSION;

const int32 GEngineMinNetVersion		= 7038;
const int32 GEngineNegotiationVersion	= 3077;

// Global instance of the current engine version
// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
FEngineVersion FEngineVersion::CurrentVersion(ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION, (ENGINE_CURRENT_CL_VERSION | (ENGINE_IS_LICENSEE_VERSION << 31)), VXGI_BRANCH_NAME);
#else
// NVCHANGE_END: Add VXGI
FEngineVersion FEngineVersion::CurrentVersion(ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION, (ENGINE_CURRENT_CL_VERSION | (ENGINE_IS_LICENSEE_VERSION << 31)), BRANCH_NAME);
// NVCHANGE_BEGIN: Add VXGI
#endif
// NVCHANGE_END: Add VXGI


// Global instance of the current engine version

// Version which this engine maintains strict API and package compatibility with. By default, we always maintain compatibility with the current major/minor version, unless we're built at a different changelist.
FEngineVersion FEngineVersion::CompatibleWithVersion(ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, 0, (ENGINE_COMPATIBLE_CL_VERSION | (ENGINE_IS_LICENSEE_VERSION << 31)), BRANCH_NAME);


FEngineVersionBase::FEngineVersionBase()
: Major(0)
, Minor(0)
, Patch(0)
, Changelist(0)
{
}

FEngineVersionBase::FEngineVersionBase(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist)
: Major(InMajor)
, Minor(InMinor)
, Patch(InPatch)
, Changelist(InChangelist)
{
}

uint32 FEngineVersionBase::GetChangelist() const
{
	// Mask to ignore licensee bit
	return Changelist & (uint32)0x7fffffffU;
}

bool FEngineVersionBase::IsLicenseeVersion() const
{
	// Check for licensee bit
	return (Changelist & (uint32)0x80000000U) != 0;
}

bool FEngineVersionBase::IsEmpty() const
{
	return Major == 0 && Minor == 0 && Patch == 0;
}

bool FEngineVersionBase::HasChangelist() const
{
	return GetChangelist() != 0;
}

EVersionComparison FEngineVersionBase::GetNewest(const FEngineVersionBase &First, const FEngineVersionBase &Second, EVersionComponent *OutComponent)
{
	EVersionComponent LocalComponent = EVersionComponent::Minor;
	auto& Component = OutComponent ? *OutComponent : LocalComponent;

	// Compare major versions
	if (First.GetMajor() != Second.GetMajor())
	{
		Component = EVersionComponent::Major;
		return (First.GetMajor() > Second.GetMajor()) ? EVersionComparison::First : EVersionComparison::Second;
	}

	// Compare minor versions
	if (First.GetMinor() != Second.GetMinor())
	{
		Component = EVersionComponent::Minor;
		return (First.GetMinor() > Second.GetMinor()) ? EVersionComparison::First : EVersionComparison::Second;
	}

	// Compare patch versions
	if (First.GetPatch() != Second.GetPatch())
	{
		Component = EVersionComponent::Patch;
		return (First.GetPatch() > Second.GetPatch()) ? EVersionComparison::First : EVersionComparison::Second;
	}

	// Compare changelists (only if they're both from the same vendor, and they're both valid)
	if (First.IsLicenseeVersion() == Second.IsLicenseeVersion() && First.HasChangelist() && Second.HasChangelist() && First.GetChangelist() != Second.GetChangelist())
	{
		Component = EVersionComponent::Changelist;
		return (First.GetChangelist() > Second.GetChangelist()) ? EVersionComparison::First : EVersionComparison::Second;
	}

	// Otherwise they're the same
	return EVersionComparison::Neither;
}

uint32 FEngineVersionBase::EncodeLicenseeChangelist(uint32 Changelist)
{
	return Changelist | 0x80000000;
}


FEngineVersion::FEngineVersion()
{
	Empty();
}

FEngineVersion::FEngineVersion(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch)
{
	Set(InMajor, InMinor, InPatch, InChangelist, InBranch);
}

void FEngineVersion::Set(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch)
{
	Major = InMajor;
	Minor = InMinor;
	Patch = InPatch;
	Changelist = InChangelist;
	Branch = InBranch;
}

void FEngineVersion::Empty()
{
	Set(0, 0, 0, 0, FString());
}

bool FEngineVersion::IsCompatibleWith(const FEngineVersionBase &Other) const
{
	// If this or the other is not a promoted build, always assume compatibility. 
	if(!HasChangelist() || !Other.HasChangelist())
	{
		return true;
	}
	else
	{
		return FEngineVersion::GetNewest(*this, Other, nullptr) != EVersionComparison::Second;
	}
}

FString FEngineVersion::ToString(EVersionComponent LastComponent) const
{
	FString Result = FString::Printf(TEXT("%u"), Major);
	if(LastComponent >= EVersionComponent::Minor)
	{
		Result += FString::Printf(TEXT(".%u"), Minor);
		if(LastComponent >= EVersionComponent::Patch)
		{
			Result += FString::Printf(TEXT(".%u"), Patch);
			if(LastComponent >= EVersionComponent::Changelist)
			{
				Result += FString::Printf(TEXT("-%u"), GetChangelist());
				if(LastComponent >= EVersionComponent::Branch && Branch.Len() > 0)
				{
					Result += FString::Printf(TEXT("+%s"), *Branch);
				}
			}
			// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
			else
			{
				Result += FString(TEXT("+VXGI-") TEXT(VXGI_VERSION_STRING));
			}
#endif
			// NVCHANGE_END: Add VXGI
		}
	}
	Result += FString(TEXT("-custom"));
	return Result;
}

bool FEngineVersion::Parse(const FString &Text, FEngineVersion &OutVersion)
{
	TCHAR *End;

	// Read the major/minor/patch numbers
	uint64 Major = FCString::Strtoui64(*Text, &End, 10);
	if(Major > MAX_uint16 || *(End++) != '.') return false;

	uint64 Minor = FCString::Strtoui64(End, &End, 10);
	if(Minor > MAX_uint16 || *(End++) != '.') return false;

	uint64 Patch = FCString::Strtoui64(End, &End, 10);
	if(Patch > MAX_uint16) return false;

	// Read the optional changelist number
	uint64 Changelist = 0;
	if(*End == '-')
	{
		End++;
		Changelist = FCString::Strtoui64(End, &End, 10);
		if(Changelist > MAX_uint32) return false;
	}

	// Read the optional branch name
	FString Branch;
	if(*End == '+')
	{
		End++;
		// read to the end of the string. There's no standard for the branch name to verify.
		Branch = FString(End);
	}

	// Build the output version
	OutVersion.Set(Major, Minor, Patch, Changelist, Branch);
	return true;
}

const FEngineVersion& FEngineVersion::Current()
{
	return CurrentVersion;
}

const FEngineVersion& FEngineVersion::CompatibleWith()
{
	return CompatibleWithVersion;
}

const FString& FEngineVersion::GetBranchDescriptor() const
{
	return Branch;
}

bool FEngineVersion::OverrideCurrentVersionChangelist(int32 NewChangelist, int32 NewCompatibleChangelist)
{
	if(CurrentVersion.GetChangelist() != 0 || CompatibleWithVersion.GetChangelist() != 0)
	{
		return false;
	}

	CurrentVersion.Set(CurrentVersion.Major, CurrentVersion.Minor, CurrentVersion.Patch, NewChangelist | (ENGINE_IS_LICENSEE_VERSION << 31), CurrentVersion.Branch);
	CompatibleWithVersion.Set(CompatibleWithVersion.Major, CompatibleWithVersion.Minor, CompatibleWithVersion.Patch, NewCompatibleChangelist | (ENGINE_IS_LICENSEE_VERSION << 31), CompatibleWithVersion.Branch);
	return true;
}

void operator<<(FArchive &Ar, FEngineVersion &Version)
{
	Ar << Version.Major;
	Ar << Version.Minor;
	Ar << Version.Patch;
	Ar << Version.Changelist;
	Ar << Version.Branch;
}

// Unique Release Object version id
const FGuid FReleaseObjectVersion::GUID(0x9C54D522, 0xA8264FBE, 0x94210746, 0x61B482D0);
// Register Release custom version with Core
FCustomVersionRegistration GRegisterReleaseObjectVersion(FReleaseObjectVersion::GUID, FReleaseObjectVersion::LatestVersion, TEXT("Release"));
