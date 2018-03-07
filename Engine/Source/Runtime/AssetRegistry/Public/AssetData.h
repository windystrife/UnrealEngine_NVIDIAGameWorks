// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "UObject/LinkerLoad.h"
#include "AssetDataTagMap.h"
#include "PrimaryAssetId.h"

#include "AssetData.generated.h"

ASSETREGISTRY_API DECLARE_LOG_CATEGORY_EXTERN(LogAssetData, Log, All);

/** Version used for serializing asset registry caches, both runtime and editor */
struct ASSETREGISTRY_API FAssetRegistryVersion
{
	enum Type
	{
		PreVersioning = 0,		// From before file versioning was implemented
		HardSoftDependencies,	// The first version of the runtime asset registry to include file versioning.
		AddAssetRegistryState,	// Added FAssetRegistryState and support for piecemeal serialization
		ChangedAssetData,		// AssetData serialization format changed, versions before this are not readable
		RemovedMD5Hash,			// Removed MD5 hash from package data

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	/** The GUID for this custom version number */
	const static FGuid GUID;

	/** Read/write the custom version to the archive, should call at the very beginning */
	static bool SerializeVersion(FArchive& Ar, FAssetRegistryVersion::Type& Version);

private:
	FAssetRegistryVersion() {}
};

/** 
 * A struct to hold important information about an assets found by the Asset Registry
 * This struct is transient and should never be serialized
 */
USTRUCT(BlueprintType)
struct FAssetData
{
	GENERATED_BODY()
public:

	/** The object path for the asset in the form PackageName.AssetName. Only top level objects in a package can have AssetData */
	UPROPERTY(BlueprintReadOnly, Category=AssetData, transient)
	FName ObjectPath;
	/** The name of the package in which the asset is found, this is the full long package name such as /Game/Path/Package */
	UPROPERTY(BlueprintReadOnly, Category=AssetData, transient)
	FName PackageName;
	/** The path to the package in which the asset is found, this is /Game/Path with the Package stripped off */
	UPROPERTY(BlueprintReadOnly, Category=AssetData, transient)
	FName PackagePath;
	/** The name of the asset without the package */
	UPROPERTY(BlueprintReadOnly, Category=AssetData, transient)
	FName AssetName;
	/** The name of the asset's class */
	UPROPERTY(BlueprintReadOnly, Category=AssetData, transient)
	FName AssetClass;
	/** The map of values for properties that were marked AssetRegistrySearchable or added by GetAssetRegistryTags */
	FAssetDataTagMapSharedView TagsAndValues;
	/** The IDs of the chunks this asset is located in for streaming install.  Empty if not assigned to a chunk */
	TArray<int32> ChunkIDs;
	/** Asset package flags */
	uint32 PackageFlags;

public:
	/** Default constructor */
	FAssetData()
	{}

	/** Constructor */
	FAssetData(FName InPackageName, FName InPackagePath, FName InAssetName, FName InAssetClass, FAssetDataTagMap InTags = FAssetDataTagMap(), TArray<int32> InChunkIDs = TArray<int32>(), uint32 InPackageFlags = 0)
		: PackageName(InPackageName)
		, PackagePath(InPackagePath)
		, AssetName(InAssetName)
		, AssetClass(InAssetClass)
		, TagsAndValues(MoveTemp(InTags))
		, ChunkIDs(MoveTemp(InChunkIDs))
		, PackageFlags(InPackageFlags)
	{
		FString ObjectPathStr = PackageName.ToString() + TEXT(".");

		ObjectPathStr += AssetName.ToString();

		ObjectPath = FName(*ObjectPathStr);
	}

	/** Constructor taking a UObject. By default trying to create one for a blueprint class will create one for the UBlueprint instead, but this can be overridden */
	FAssetData(const UObject* InAsset, bool bAllowBlueprintClass = false)
	{
		if ( InAsset != nullptr )
		{
			const UClass* InClass = Cast<UClass>(InAsset);
			if (InClass && InClass->ClassGeneratedBy && !bAllowBlueprintClass)
			{
				// For Blueprints, the AssetData refers to the UBlueprint and not the UBlueprintGeneratedClass
				InAsset = InClass->ClassGeneratedBy;
			}

			const UPackage* Outermost = InAsset->GetOutermost();
			const UObject* Outer = InAsset->GetOuter();

			PackageName = Outermost->GetFName();
			PackagePath = FName(*FPackageName::GetLongPackagePath(Outermost->GetName()));
			AssetName = InAsset->GetFName();
			AssetClass = InAsset->GetClass()->GetFName();
			ObjectPath = FName(*InAsset->GetPathName());

			TArray<UObject::FAssetRegistryTag> ObjectTags;
			InAsset->GetAssetRegistryTags(ObjectTags);

			FAssetDataTagMap NewTagsAndValues;
			for (UObject::FAssetRegistryTag& AssetRegistryTag : ObjectTags)
			{
				if (AssetRegistryTag.Name != NAME_None && !AssetRegistryTag.Value.IsEmpty())
				{
					// Don't add empty tags
					NewTagsAndValues.Add(AssetRegistryTag.Name, AssetRegistryTag.Value);
				}
			}

			TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(NewTagsAndValues));
			ChunkIDs = Outermost->GetChunkIDs();
			PackageFlags = Outermost->GetPackageFlags();
		}
	}

	/** FAssetDatas are equal if their object paths match */
	bool operator==(const FAssetData& Other) const
	{
		return ObjectPath == Other.ObjectPath;
	}

	bool operator!=(const FAssetData& Other) const
	{
		return ObjectPath != Other.ObjectPath;
	}

	bool operator>(const FAssetData& Other) const
	{
		return ObjectPath > Other.ObjectPath;
	}

	bool operator<(const FAssetData& Other) const
	{
		return ObjectPath < Other.ObjectPath;
	}

	/** Checks to see if this AssetData refers to an asset or is NULL */
	bool IsValid() const
	{
		return ObjectPath != NAME_None;
	}

	/** Returns true if this asset was found in a UAsset file */
	bool IsUAsset() const
	{
		return FPackageName::GetLongPackageAssetName(PackageName.ToString()) == AssetName.ToString();
	}

	/** Returns the full name for the asset in the form: Class ObjectPath */
	FString GetFullName() const
	{
		FString FullName;
		GetFullName(FullName);
		return FullName;
	}

	/** Populates OutFullName with the full name for the asset in the form: Class ObjectPath */
	void GetFullName(FString& OutFullName) const
	{
		OutFullName.Reset();
		AssetClass.AppendString(OutFullName);
		OutFullName.AppendChar(' ');
		ObjectPath.AppendString(OutFullName);
	}

	/** Returns the name for the asset in the form: Class'ObjectPath' */
	FString GetExportTextName() const
	{
		FString ExportTextName;
		GetExportTextName(ExportTextName);
		return ExportTextName;
	}

	/** Populates OutExportTextName with the name for the asset in the form: Class'ObjectPath' */
	void GetExportTextName(FString& OutExportTextName) const
	{
		OutExportTextName.Reset();
		AssetClass.AppendString(OutExportTextName);
		OutExportTextName.AppendChar('\'');
		ObjectPath.AppendString(OutExportTextName);
		OutExportTextName.AppendChar('\'');
	}

	/** Returns true if the this asset is a redirector. */
	bool IsRedirector() const
	{
		if ( AssetClass == UObjectRedirector::StaticClass()->GetFName() )
		{
			return true;
		}

		return false;
	}

	/** Returns the class UClass if it is loaded. It is not possible to load the class if it is unloaded since we only have the short name. */
	UClass* GetClass() const
	{
		if ( !IsValid() )
		{
			// Dont even try to find the class if the objectpath isn't set
			return NULL;
		}

		UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *AssetClass.ToString());

		if (!FoundClass)
		{
			// Look for class redirectors
			FName NewPath = FLinkerLoad::FindNewNameForClass(AssetClass, false);

			if (NewPath != NAME_None)
			{
				FoundClass = FindObject<UClass>(ANY_PACKAGE, *NewPath.ToString());
			}
		}
		return FoundClass;
	}

	/** Convert to a SoftObjectPath for loading */
	FSoftObjectPath ToSoftObjectPath() const
	{
		return FSoftObjectPath(ObjectPath.ToString());
	}

	DEPRECATED(4.18, "ToStringReference was renamed to ToSoftObjectPath")
	FSoftObjectPath ToStringReference() const
	{
		return ToSoftObjectPath();
	}
	
	/** Gets primary asset id of this data */
	FPrimaryAssetId GetPrimaryAssetId() const
	{
		FName PrimaryAssetType, PrimaryAssetName;
		GetTagValueNameImpl(FPrimaryAssetId::PrimaryAssetTypeTag, PrimaryAssetType);
		GetTagValueNameImpl(FPrimaryAssetId::PrimaryAssetNameTag, PrimaryAssetName);

		if (PrimaryAssetType != NAME_None && PrimaryAssetName != NAME_None)
		{
			return FPrimaryAssetId(PrimaryAssetType, PrimaryAssetName);
		}

		return FPrimaryAssetId();
	}

	/** Returns the asset UObject if it is loaded or loads the asset if it is unloaded then returns the result */
	UObject* GetAsset() const
	{
		if ( !IsValid() )
		{
			// Dont even try to find the object if the objectpath isn't set
			return NULL;
		}

		UObject* Asset = FindObject<UObject>(NULL, *ObjectPath.ToString());
		if ( Asset == NULL )
		{
			Asset = LoadObject<UObject>(NULL, *ObjectPath.ToString());
		}

		return Asset;
	}

	UPackage* GetPackage() const
	{
		if (PackageName == NAME_None)
		{
			return NULL;
		}

		UPackage* Package = FindPackage(NULL, *PackageName.ToString());
		if (Package)
		{
			Package->FullyLoad();
		}
		else
		{
			Package = LoadPackage(NULL, *PackageName.ToString(), LOAD_None);
		}

		return Package;
	}

	/** Try and get the value associated with the given tag as a type converted value */
	template <typename ValueType>
	bool GetTagValue(const FName InTagName, ValueType& OutTagValue) const;

	/** Try and get the value associated with the given tag as a type converted value, or an empty value if it doesn't exist */
	template <typename ValueType>
	ValueType GetTagValueRef(const FName InTagName) const;

	/** Returns true if the asset is loaded */
	bool IsAssetLoaded() const
	{
		return IsValid() && FindObject<UObject>(NULL, *ObjectPath.ToString()) != NULL;
	}

	/** Prints the details of the asset to the log */
	void PrintAssetData() const
	{
		UE_LOG(LogAssetData, Log, TEXT("    FAssetData for %s"), *ObjectPath.ToString());
		UE_LOG(LogAssetData, Log, TEXT("    ============================="));
		UE_LOG(LogAssetData, Log, TEXT("        PackageName: %s"), *PackageName.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        PackagePath: %s"), *PackagePath.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        AssetName: %s"), *AssetName.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        AssetClass: %s"), *AssetClass.ToString());
		UE_LOG(LogAssetData, Log, TEXT("        TagsAndValues: %d"), TagsAndValues.Num());

		for (const auto& TagValue: TagsAndValues)
		{
			UE_LOG(LogAssetData, Log, TEXT("            %s : %s"), *TagValue.Key.ToString(), *TagValue.Value);
		}

		UE_LOG(LogAssetData, Log, TEXT("        ChunkIDs: %d"), ChunkIDs.Num());

		for (int32 Chunk: ChunkIDs)
		{
			UE_LOG(LogAssetData, Log, TEXT("                 %d"), Chunk);
		}

		UE_LOG(LogAssetData, Log, TEXT("        PackageFlags: %d"), PackageFlags);
	}

	/** Get the first FAssetData of a particular class from an Array of FAssetData */
	static FAssetData GetFirstAssetDataOfClass(const TArray<FAssetData>& Assets, const UClass* DesiredClass)
	{
		for(int32 AssetIdx=0; AssetIdx<Assets.Num(); AssetIdx++)
		{
			const FAssetData& Data = Assets[AssetIdx];
			UClass* AssetClass = Data.GetClass();
			if( AssetClass != NULL && AssetClass->IsChildOf(DesiredClass) )
			{
				return Data;
			}
		}
		return FAssetData();
	}

	/** Convenience template for finding first asset of a class */
	template <class T>
	static T* GetFirstAsset(const TArray<FAssetData>& Assets)
	{
		UClass* DesiredClass = T::StaticClass();
		UObject* Asset = FAssetData::GetFirstAssetDataOfClass(Assets, DesiredClass).GetAsset();
		check(Asset == NULL || Asset->IsA(DesiredClass));
		return (T*)Asset;
	}

	/** 
	 * Serialize as part of the registry cache. This is not meant to be serialized as part of a package so  it does not handle versions normally
	 * To version this data change FAssetRegistryVersion
	 */
	void SerializeForCache(FArchive& Ar)
	{
		// Serialize out the asset info
		Ar << ObjectPath;
		Ar << PackagePath;
		Ar << AssetClass;

		// These are derived from ObjectPath, we manually serialize them because they get pooled
		Ar << PackageName;
		Ar << AssetName;

		Ar << TagsAndValues;
		Ar << ChunkIDs;
		Ar << PackageFlags;
	}

private:
	bool GetTagValueStringImpl(const FName InTagName, FString& OutTagValue) const
	{
		if (const FString* FoundValue = TagsAndValues.Find(InTagName))
		{
			bool bIsHandled = false;
			if (FTextStringHelper::IsComplexText(**FoundValue))
			{
				FText TmpText;
				if (FTextStringHelper::ReadFromString(**FoundValue, TmpText))
				{
					bIsHandled = true;
					OutTagValue = TmpText.ToString();
				}
			}

			if (!bIsHandled)
			{
				OutTagValue = *FoundValue;
			}

			return true;
		}

		return false;
	}

	bool GetTagValueTextImpl(const FName InTagName, FText& OutTagValue) const
	{
		if (const FString* FoundValue = TagsAndValues.Find(InTagName))
		{
			if (!FTextStringHelper::ReadFromString(**FoundValue, OutTagValue))
			{
				OutTagValue = FText::FromString(*FoundValue);
			}
			return true;
		}
		return false;
	}

	bool GetTagValueNameImpl(const FName InTagName, FName& OutTagValue) const
	{
		FString StrValue;
		if (GetTagValueStringImpl(InTagName, StrValue))
		{
			OutTagValue = *StrValue;
			return true;
		}
		return false;
	}
};


template<>
struct TStructOpsTypeTraits<FAssetData> : public TStructOpsTypeTraitsBase2<FAssetData>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

template <typename ValueType>
inline bool FAssetData::GetTagValue(const FName InTagName, ValueType& OutTagValue) const
{
	if (const FString* FoundValue = TagsAndValues.Find(InTagName))
	{
		FMemory::Memzero(&OutTagValue, sizeof(ValueType));
		Lex::FromString(OutTagValue, **FoundValue);
		return true;
	}
	return false;
}

template <>
inline bool FAssetData::GetTagValue<FString>(const FName InTagName, FString& OutTagValue) const
{
	return GetTagValueStringImpl(InTagName, OutTagValue);
}

template <>
inline bool FAssetData::GetTagValue<FText>(const FName InTagName, FText& OutTagValue) const
{
	return GetTagValueTextImpl(InTagName, OutTagValue);
}

template <>
inline bool FAssetData::GetTagValue<FName>(const FName InTagName, FName& OutTagValue) const
{
	return GetTagValueNameImpl(InTagName, OutTagValue);
}

template <typename ValueType>
inline ValueType FAssetData::GetTagValueRef(const FName InTagName) const
{
	ValueType TmpValue;
	FMemory::Memzero(&TmpValue, sizeof(ValueType));
	if (const FString* FoundValue = TagsAndValues.Find(InTagName))
	{
		Lex::FromString(TmpValue, **FoundValue);
	}
	return TmpValue;
}

template <>
inline FString FAssetData::GetTagValueRef<FString>(const FName InTagName) const
{
	FString TmpValue;
	GetTagValueStringImpl(InTagName, TmpValue);
	return TmpValue;
}

template <>
inline FText FAssetData::GetTagValueRef<FText>(const FName InTagName) const
{
	FText TmpValue;
	GetTagValueTextImpl(InTagName, TmpValue);
	return TmpValue;
}

template <>
inline FName FAssetData::GetTagValueRef<FName>(const FName InTagName) const
{
	FName TmpValue;
	GetTagValueNameImpl(InTagName, TmpValue);
	return TmpValue;
}

/** A class to hold data about a package on disk, this data is updated on save/load and is not updated when an asset changes in memory */
class FAssetPackageData
{
public:
	/** Total size of this asset on disk */
	int64 DiskSize;

	/** Guid of the source package, uniquely identifies an asset package */
	FGuid PackageGuid;

	FAssetPackageData()
		: DiskSize(0)
	{
	}

	/**
	 * Serialize as part of the registry cache. This is not meant to be serialized as part of a package so  it does not handle versions normally
	 * To version this data change FAssetRegistryVersion
	 */
	void SerializeForCache(FArchive& Ar)
	{
		Ar << DiskSize;
		Ar << PackageGuid;
	}
};

/** A structure defining a thing that can be reference by something else in the asset registry. Represents either a package of a primary asset id */
struct FAssetIdentifier
{
	/** The name of the package that is depended on, this is always set unless PrimaryAssetType is */
	FName PackageName;
	/** The primary asset type, if valid the ObjectName is the PrimaryAssetName */
	FPrimaryAssetType PrimaryAssetType;
	/** Specific object within a package. If empty, assumed to be the default asset */
	FName ObjectName;
	/** Name of specific value being referenced, if ObjectName specifies a type such as a UStruct */
	FName ValueName;

	/** Can be implicitly constructed from just the package name */
	FAssetIdentifier(FName InPackageName, FName InObjectName = NAME_None, FName InValueName = NAME_None)
		: PackageName(InPackageName), PrimaryAssetType(NAME_None), ObjectName(InObjectName), ValueName(InValueName)
	{}

	/** Construct from a primary asset id */
	FAssetIdentifier(const FPrimaryAssetId& PrimaryAssetId, FName InValueName = NAME_None)
		: PackageName(NAME_None), PrimaryAssetType(PrimaryAssetId.PrimaryAssetType), ObjectName(PrimaryAssetId.PrimaryAssetName), ValueName(InValueName)
	{}

	FAssetIdentifier(UObject* SourceObject, FName InValueName)
	{
		if (SourceObject)
		{
			UPackage* Package = SourceObject->GetOutermost();
			PackageName = Package->GetFName();
			ObjectName = SourceObject->GetFName();
			ValueName = InValueName;
		}
	}

	FAssetIdentifier()
		: PackageName(NAME_None), PrimaryAssetType(NAME_None), ObjectName(NAME_None), ValueName(NAME_None)
	{}

	/** Returns primary asset id for this identifier, if valid */
	FPrimaryAssetId GetPrimaryAssetId() const
	{
		if (PrimaryAssetType != NAME_None)
		{
			return FPrimaryAssetId(PrimaryAssetType, ObjectName);
		}
		return FPrimaryAssetId();
	}

	/** Returns true if this represents a package */
	bool IsPackage() const
	{
		return PackageName != NAME_None && !IsObject() && !IsValue();
	}

	/** Returns true if this represents an object, true for both package objects and PrimaryAssetId objects */
	bool IsObject() const
	{
		return ObjectName != NAME_None && !IsValue();
	}

	/** Returns true if this represents a specific value */
	bool IsValue() const
	{
		return ValueName != NAME_None;
	}

	/** Returns true if this is a valid non-null identifier */
	bool IsValid() const
	{
		return PackageName != NAME_None || GetPrimaryAssetId().IsValid();
	}

	/** Returns string version of this identifier in Package.Object::Name format */
	FString ToString() const
	{
		FString Result;
		if (PrimaryAssetType != NAME_None)
		{
			Result = GetPrimaryAssetId().ToString();
		}
		else
		{
			Result = PackageName.ToString();
			if (ObjectName != NAME_None)
			{
				Result += TEXT(".");
				Result += ObjectName.ToString();
			}
		}
		if (ValueName != NAME_None)
		{
			Result += TEXT("::");
			Result += ValueName.ToString();
		}
		return Result;
	}

	/** Converts from Package.Object::Name format */
	static FAssetIdentifier FromString(const FString& String)
	{
		// To right of :: is value
		FString PackageString;
		FString ObjectString;
		FString ValueString;

		// Try to split value out
		if (!String.Split(TEXT("::"), &PackageString, &ValueString))
		{
			PackageString = String;
		}

		// Check if it's a valid primary asset id
		FPrimaryAssetId PrimaryId = FPrimaryAssetId::FromString(PackageString);

		if (PrimaryId.IsValid())
		{
			return FAssetIdentifier(PrimaryId, *ValueString);
		}

		// Try to split on first . , if it fails PackageString will stay the same
		FString(PackageString).Split(TEXT("."), &PackageString, &ObjectString);

		return FAssetIdentifier(*PackageString, *ObjectString, *ValueString);
	}

	friend inline bool operator==(const FAssetIdentifier& A, const FAssetIdentifier& B)
	{
		return A.PackageName == B.PackageName && A.ObjectName == B.ObjectName && A.ValueName == B.ValueName;
	}

	friend inline uint32 GetTypeHash(const FAssetIdentifier& Key)
	{
		uint32 Hash = 0;

		// Most of the time only packagename is set
		if (Key.ObjectName.IsNone() && Key.ValueName.IsNone())
		{
			return GetTypeHash(Key.PackageName);
		}

		Hash = HashCombine(Hash, GetTypeHash(Key.PackageName));
		Hash = HashCombine(Hash, GetTypeHash(Key.PrimaryAssetType));
		Hash = HashCombine(Hash, GetTypeHash(Key.ObjectName));
		Hash = HashCombine(Hash, GetTypeHash(Key.ValueName));
		return Hash;
	}

	/** Identifiers may be serialized as part of the registry cache, or in other contexts. If you make changes here you must also change FAssetRegistryVersion */
	friend FArchive& operator<<(FArchive& Ar, FAssetIdentifier& AssetIdentifier)
	{
		// Serialize bitfield of which elements to serialize, in general many are empty
		uint8 FieldBits = 0;

		if (Ar.IsSaving())
		{
			FieldBits |= (AssetIdentifier.PackageName != NAME_None) << 0;
			FieldBits |= (AssetIdentifier.PrimaryAssetType != NAME_None) << 1;
			FieldBits |= (AssetIdentifier.ObjectName != NAME_None) << 2;
			FieldBits |= (AssetIdentifier.ValueName != NAME_None) << 3;
		}

		Ar << FieldBits;

		if (FieldBits & (1 << 0))
		{
			Ar << AssetIdentifier.PackageName;
		}
		if (FieldBits & (1 << 1))
		{
			FName TypeName = AssetIdentifier.PrimaryAssetType.GetName();
			Ar << TypeName;

			if (Ar.IsLoading())
			{
				AssetIdentifier.PrimaryAssetType = TypeName;
			}
		}
		if (FieldBits & (1 << 2))
		{
			Ar << AssetIdentifier.ObjectName;
		}
		if (FieldBits & (1 << 3))
		{
			Ar << AssetIdentifier.ValueName;
		}
		
		return Ar;
	}
};

