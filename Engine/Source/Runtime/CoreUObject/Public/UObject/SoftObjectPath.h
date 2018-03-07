// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/Class.h"
#include "HAL/ThreadSingleton.h"

/**
 * A struct that contains a string reference to an object, either a top level asset or a subobject.
 * This can be used to make soft references to assets that are loaded on demand.
 * This is stored internally as an FName pointing to the top level asset (/package/path.assetname) and an option a string subobject path.
 * If the MetaClass metadata is applied to a UProperty with this the UI will restrict to that type of asset.
 */
struct COREUOBJECT_API FSoftObjectPath
{
	FSoftObjectPath() {}

	/** Construct from another soft object path */
	FSoftObjectPath(const FSoftObjectPath& Other)
		: AssetPathName(Other.AssetPathName)
		, SubPathString(Other.SubPathString)
	{
	}

	/** Construct from a path string */
	FSoftObjectPath(FString PathString)
	{
		SetPath(MoveTemp(PathString));
	}

	/** Construct from an asset FName and subobject pair */
	FSoftObjectPath(FName InAssetPathName, FString InSubPathString)
		: AssetPathName(InAssetPathName)
		, SubPathString(InSubPathString)
	{}
	
	/** Construct from an existing object in memory */
	FSoftObjectPath(const UObject* InObject);

	~FSoftObjectPath() {}

	/** Returns string representation of reference, in form /package/path.assetname[:subpath] */
	FString ToString() const;

	/** Returns the entire asset path as an FName, including both package and asset but not sub object */
	FORCEINLINE FName GetAssetPathName() const
	{
		return AssetPathName;
	}

	/** Returns string version of asset path, including both package and asset but not sub object */
	FORCEINLINE FString GetAssetPathString() const
	{
		if (AssetPathName == NAME_None)
		{
			return FString();
		}

		return AssetPathName.ToString();
	}

	/** Returns the sub path, which is often empty */
	FORCEINLINE FString GetSubPathString() const
	{
		return SubPathString;
	}

	/** Returns /package/path, leaving off the asset name and sub object */
	FString GetLongPackageName() const
	{
		FString PackageName;
		GetAssetPathString().Split(TEXT("."), &PackageName, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		return PackageName;
	}

	/** Returns assetname string, leaving off the /package/path part and sub object */
	FString GetAssetName() const
	{
		FString AssetName;
		GetAssetPathString().Split(TEXT("."), nullptr, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		return AssetName;
	}

	/** Sets asset path of this reference based on a string path */
	void SetPath(FString Path);

	/**
	 * Attempts to load the asset, this will call LoadObject which can be very slow
	 *
	 * @return Loaded UObject, or nullptr if the reference is null or the asset fails to load
	 */
	UObject* TryLoad() const;

	/**
	 * Attempts to find a currently loaded object that matches this path
	 *
	 * @return Found UObject, or nullptr if not currently in memory
	 */
	UObject* ResolveObject() const;

	/** Resets reference to point to null */
	void Reset()
	{		
		AssetPathName = NAME_None;
		SubPathString.Reset();
	}
	
	/** Check if this could possibly refer to a real object, or was initialized to null */
	FORCEINLINE bool IsValid() const
	{
		return AssetPathName != NAME_None;
	}

	/** Checks to see if this is initialized to null */
	FORCEINLINE bool IsNull() const
	{
		return AssetPathName == NAME_None;
	}

	/** Check if this represents an asset, meaning it is not null but does not have a sub path */
	FORCEINLINE bool IsAsset() const
	{
		return AssetPathName != NAME_None && SubPathString.IsEmpty();
	}

	/** Check if this represents a sub object, meaning it has a sub path */
	FORCEINLINE bool IsSubobject() const
	{
		return AssetPathName != NAME_None && !SubPathString.IsEmpty();
	}

	/** Struct overrides */
	bool Serialize(FArchive& Ar);
	bool operator==(FSoftObjectPath const& Other) const;
	bool operator!=(FSoftObjectPath const& Other) const
	{
		return !(*this == Other);
	}
	FSoftObjectPath& operator=(FSoftObjectPath const& Other);
	bool ExportTextItem(FString& ValueStr, FSoftObjectPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	bool ImportTextItem( const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText );
	bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar);

	/** Serializes the internal path and also handles save/PIE fixups. Call this from the archiver overrides */
	void SerializePath(FArchive& Ar, bool bSkipSerializeIfArchiveHasSize = false);

	/** Fixes up string asset reference for saving, call if saving with a method that skips SerializePath. This can modify the path, it will return true if it was modified */
	bool PreSavePath();

	/** Handles when a string asset reference has been loaded, call if loading with a method that skips SerializePath. This does not modify path but might call callbacks */
	void PostLoadPath() const;

	/** Fixes up this SoftObjectPath to add or remove the PIE prefix depending on what is currently active */
	void FixupForPIE();

	FORCEINLINE friend uint32 GetTypeHash(FSoftObjectPath const& This)
	{
		uint32 Hash = 0;

		Hash = HashCombine(Hash, GetTypeHash(This.AssetPathName));
		Hash = HashCombine(Hash, GetTypeHash(This.SubPathString));
		return Hash;
	}

	/** Code needed by FSoftObjectPtr internals */
	static int32 GetCurrentTag()
	{
		return CurrentTag.GetValue();
	}
	static int32 InvalidateTag()
	{
		return CurrentTag.Increment();
	}
	static FSoftObjectPath GetOrCreateIDForObject(const UObject* Object);
	
	/** Adds list of packages names that have been created specifically for PIE, this is used for editor fixup */
	static void AddPIEPackageName(FName NewPIEPackageName);
	
	/** Disables special PIE path handling, call when PIE finishes to clear list */
	static void ClearPIEPackageNames();

private:
	/** Asset path, patch to a top level object in a package. This is /package/path.assetname */
	FName AssetPathName;

	/** Optional FString for subobject within an asset. This is the sub path after the : */
	FString SubPathString;

	/** Global counter that determines when we need to re-search based on path because more objects have been loaded **/
	static FThreadSafeCounter CurrentTag;

	/** Package names currently being duplicated, needed by FixupForPIE */
	static TSet<FName> PIEPackageNames;

	friend COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FSoftObjectPath();
};

/**
 * A struct that contains a string reference to a class, can be used to make soft references to classes
 */
struct COREUOBJECT_API FSoftClassPath : public FSoftObjectPath
{
	FSoftClassPath()
	{ }

	FSoftClassPath(FSoftClassPath const& Other)
		: FSoftObjectPath(Other)
	{ }

	/**
	 * Construct from a path string
	 */
	FSoftClassPath(const FString& PathString)
		: FSoftObjectPath(PathString)
	{ }

	/**
	 * Construct from an existing class, will do some string processing
	 */
	FSoftClassPath(const UClass* InClass)
		: FSoftObjectPath(InClass)
	{ }

	/**
	* Attempts to load the class.
	* @return Loaded UObject, or null if the class fails to load, or if the reference is not valid.
	*/
	template<typename T>
	UClass* TryLoadClass() const
	{
		if ( IsValid() )
		{
			return LoadClass<T>(nullptr, *ToString(), nullptr, LOAD_None, nullptr);
		}

		return nullptr;
	}

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 * @return Found UClass, or NULL if not currently loaded
	 */
	UClass* ResolveClass() const;

	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar);

	static FSoftClassPath GetOrCreateIDForClass(const UClass *InClass);

private:
	/** Forbid creation for UObject. This class is for UClass only. Use FSoftObjectPath instead. */
	FSoftClassPath(const UObject* InObject) { }

	/** Forbidden. This class is for UClass only. Use FSoftObjectPath instead. */
	static FSoftObjectPath GetOrCreateIDForObject(const UObject *Object);
};

// Not deprecating these yet as it will lead to too many warnings in games
//DEPRECATED(4.18, "FStringAssetReference was renamed to FSoftObjectPath as it is now not always a string and can also refer to a subobject")
typedef FSoftObjectPath FStringAssetReference;

//DEPRECATED(4.18, "FStringClassReference was renamed to FSoftClassPath")
typedef FSoftClassPath FStringClassReference;

/** OPtions for how to set soft object path collection */
enum class ESoftObjectPathCollectType : uint8
{
	/** References is not tracked in any situation, transient reference */
	NeverCollect,
	/** Editor only reference, this is tracked for redirector fixup but not for cooking */
	EditorOnlyCollect,
	/** Game reference, this is gathered for both redirector fixup and cooking */
	AlwaysCollect,
};

class COREUOBJECT_API FSoftObjectPathThreadContext : public TThreadSingleton<FSoftObjectPathThreadContext>
{
	friend TThreadSingleton<FSoftObjectPathThreadContext>;
	friend struct FSoftObjectPathSerializationScope;

	FSoftObjectPathThreadContext() {}

	struct FSerializationOptions
	{
		FName PackageName;
		FName PropertyName;
		ESoftObjectPathCollectType CollectType;

		FSerializationOptions() : CollectType(ESoftObjectPathCollectType::AlwaysCollect) {}
		FSerializationOptions(FName InPackageName, FName InPropertyName, ESoftObjectPathCollectType InCollectType) : PackageName(InPackageName), PropertyName(InPropertyName), CollectType(InCollectType) {}
	};

	TArray<FSerializationOptions> OptionStack;
public:
	/** 
	 * Returns the current serialization options that were added using SerializationScope or LinkerLoad
	 *
	 * @param OutPackageName Package that this string asset belongs to
	 * @param OutPropertyName Property that this string asset reference belongs to
	 * @param OutCollectType Type of collecting that should be done
	 */
	bool GetSerializationOptions(FName& OutPackageName, FName& OutPropertyName, ESoftObjectPathCollectType& OutCollectType) const;
};

/** Helper class to set and restore serialization options for string asset references */
struct FSoftObjectPathSerializationScope
{
	/** 
	 * Create a new serialization scope, which affects the way that string asset references are saved
	 *
	 * @param SerializingPackageName Package that this string asset belongs to
	 * @param SerializingPropertyName Property that this string asset reference belongs to
	 * @param CollectType Set type of collecting that should be done, can be used to disable tracking entirely
	 */
	FSoftObjectPathSerializationScope(FName SerializingPackageName, FName SerializingPropertyName, ESoftObjectPathCollectType CollectType)
	{
		FSoftObjectPathThreadContext::Get().OptionStack.Emplace(SerializingPackageName, SerializingPropertyName, CollectType);
	}

	~FSoftObjectPathSerializationScope()
	{
		FSoftObjectPathThreadContext::Get().OptionStack.Pop();
	}
};