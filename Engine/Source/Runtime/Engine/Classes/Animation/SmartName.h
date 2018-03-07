// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "BoneContainer.h"
#include "AnimPhysObjectVersion.h"
#include "SmartName.generated.h"

struct FSmartName;

/** in the future if we need more bools, please convert to bitfield 
 * These are not saved in asset but per skeleton. 
 */
USTRUCT()
struct ENGINE_API FAnimCurveType
{
	GENERATED_USTRUCT_BODY()

	bool bMaterial;
	bool bMorphtarget;

	FAnimCurveType(bool bInMorphtarget = false, bool bInMaterial = false)
		: bMaterial(bInMaterial)
		, bMorphtarget(bInMorphtarget)
	{
	}
};

/** Curve Meta Data for each name
 * Unfortunately this should be linked to FName, but no GUID because we don't have GUID in run-time
 * We only add this if anything changed, by default, it is attribute curve
 */
USTRUCT()
struct FCurveMetaData
{
	GENERATED_USTRUCT_BODY()

	struct FAnimCurveType Type;
	/** connected bones to this meta data */
	TArray<struct FBoneReference> LinkedBones;
	/* max LOD (lowest LOD) to evaluate this. -1 means it will evaluate all the time. */
	uint8 MaxLOD;

	friend FArchive& operator<<(FArchive& Ar, FCurveMetaData& B)
	{
		Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);

		Ar << B.Type.bMaterial;
		Ar << B.Type.bMorphtarget;
		Ar << B.LinkedBones;

		if (Ar.CustomVer(FAnimPhysObjectVersion::GUID) >= FAnimPhysObjectVersion::AddLODToCurveMetaData)
		{
			Ar << B.MaxLOD;
		}

		return Ar;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	FCurveMetaData()
		: MaxLOD(0xFF)
	{
	}
};

USTRUCT()
struct ENGINE_API FSmartNameMapping
{
	GENERATED_USTRUCT_BODY();

	FSmartNameMapping();

	// Add a name to the mapping, fails if it already exists
	// @param InName - The name to add
	// @return FSmartName - populated smart name
	FSmartName AddName(FName InName);

	// Get a name from the mapping
	// @param Uid - SmartName::UID_Type of the name to retrieve
	// @param OUT OutName - Retrieved name
	// @return bool - true if name existed and OutName is valid
	bool GetName(const SmartName::UID_Type& Uid, FName& OutName) const;
	
	// Fill an array with all used UIDs
	// @param Array - Array to fill
	void FillUidArray(TArray<SmartName::UID_Type>& Array) const;

	// Fill an array with all used names
	// @param Array - Array to fill
	void FillNameArray(TArray<FName>& Array) const;

#if WITH_EDITOR
	// Change a name
	// @param Uid - SmartName::UID_Type of the name to change
	// @param NewName - New name to set 
	// @return bool - true if the name was found and changed, false if the name wasn't present in the mapping
	bool Rename(const SmartName::UID_Type& Uid, FName NewName);

	// Remove a name from the mapping
	// @param Uid - SmartName::UID_Type of the name to remove
	// @return bool - true if the name was found and removed, false if the name wasn't present in the mapping
	bool Remove(const SmartName::UID_Type& Uid);

	// Remove a name from the mapping
	// @param Uid - SmartName::UID_Type of the name to remove
	// @return bool - true if the name was found and removed, false if the name wasn't present in the mapping
	bool Remove(const FName& Name);
#endif

	// Return SmartName::UID_Type if it finds it
	// @param Name - Name of curve to find UID for
	// @return SmartName::UID_Type - MaxUID if it doesn't find, actual UID if it finds. 
	SmartName::UID_Type FindUID(const FName& Name) const;

	// Check whether a name already exists in the mapping
	// @param Uid - the SmartName::UID_Type to check
	// @return bool - whether the name was found
	bool Exists(const SmartName::UID_Type& Uid) const;

	// Check whether a name already exists in the mapping
	// @param Name - the name to check
	// @return bool - whether the name was found
	bool Exists(const FName& Name) const;

	// Find Smart Names
	bool FindSmartName(FName Name, FSmartName& OutName) const;
	bool FindSmartNameByUID(SmartName::UID_Type UID, FSmartName& OutName) const;

	// Curve Meta Data Accessors
	FCurveMetaData* GetCurveMetaData(FName CurveName)
	{
		checkSlow(Exists(CurveName));
		return &CurveMetaDataMap.FindOrAdd(CurveName);
	}

	const FCurveMetaData* GetCurveMetaData(FName CurveName) const
	{
		checkSlow(Exists(CurveName));
		return CurveMetaDataMap.Find(CurveName);
	}

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameMapping& Elem);

	/* initialize curve meta data for the container */
	void InitializeCurveMetaData(class USkeleton* Skeleton);
private:
	
	TArray<FName> CurveNameList; // List of curve names, order gives UID
	TMap<FName, FCurveMetaData> CurveMetaDataMap;
};

USTRUCT()
struct ENGINE_API FSmartNameContainer
{
	GENERATED_USTRUCT_BODY();

	// Add a new smartname container with the provided name
	void AddContainer(FName NewContainerName);

	// Get a container by name	
	const FSmartNameMapping* GetContainer(FName ContainerName) const;

	// Serialize this to the provided archive; required for TMap serialization
	void Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartNameContainer& Elem);

	/** Only restricted classes can access the protected interface */
	friend class USkeleton;
protected:
	FSmartNameMapping* GetContainerInternal(const FName& ContainerName);
	const FSmartNameMapping* GetContainerInternal(const FName& ContainerName) const;

private:
	TMap<FName, FSmartNameMapping> NameMappings;	// List of smartname mappings
};

USTRUCT()
struct ENGINE_API FSmartName
{
	GENERATED_USTRUCT_BODY();

	// name 
	UPROPERTY(VisibleAnywhere, Category=FSmartName)
	FName DisplayName;

	// SmartName::UID_Type - for faster access
	SmartName::UID_Type	UID;

	FSmartName()
		: DisplayName(NAME_None)
		, UID(SmartName::MaxUID)
	{}

	FSmartName(const FName& InDisplayName, SmartName::UID_Type InUID)
		: DisplayName(InDisplayName)
		, UID(InUID)
	{}

	bool operator==(FSmartName const& Other) const
	{
		return (DisplayName == Other.DisplayName && UID == Other.UID);
	}
	bool operator!=(const FSmartName& Other) const
	{
		return !(*this == Other);
	}
	/**
	* Serialize the SmartName
	*
	* @param Ar	Archive to serialize to
	*
	* @return True if the container was serialized
	*/
	bool Serialize(FArchive& Ar);

	friend FArchive& operator<<(FArchive& Ar, FSmartName& P)
	{
		P.Serialize(Ar);
		return Ar;
	}

	bool IsValid() const
	{
		return UID != SmartName::MaxUID;
	}
};

template<>
struct TStructOpsTypeTraits<FSmartName> : public TStructOpsTypeTraitsBase2<FSmartName>
{
	enum
	{
		WithSerializer = true,
		WithIdenticalViaEquality = true
	};
};
