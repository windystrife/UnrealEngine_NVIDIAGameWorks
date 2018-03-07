// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectBase.h: Unreal UObject base class
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("STAT_UObjectsStatGroupTester"), STAT_UObjectsStatGroupTester, STATGROUP_UObjects, COREUOBJECT_API);

class COREUOBJECT_API UObjectBase
{
	friend class UObjectBaseUtility;
	friend COREUOBJECT_API class UClass* Z_Construct_UClass_UObject();
	friend class FUObjectArray; // for access to InternalIndex without revealing it to anyone else
	friend class FUObjectAllocator; // for access to destructor without revealing it to anyone else
	friend COREUOBJECT_API void UObjectForceRegistration(UObjectBase* Object);
	friend COREUOBJECT_API void InitializePrivateStaticClass(
		class UClass* TClass_Super_StaticClass,
		class UClass* TClass_PrivateStaticClass,
		class UClass* TClass_WithinClass_StaticClass,
		const TCHAR* PackageName,
		const TCHAR* Name
		);
protected:
	UObjectBase() :
		 NamePrivate(NoInit)  // screwy, but the name was already set and we don't want to set it again
	{
	}
	/**
	 * Constructor used for bootstrapping
	 * @param	InFlags			RF_Flags to assign
	 */
	UObjectBase( EObjectFlags InFlags );
public:
	/**
	 * Constructor used by StaticAllocateObject
	 * @param	InClass				non NULL, this gives the class of the new object, if known at this time
	 * @param	InFlags				RF_Flags to assign
	 * @param	InInternalFlags EInternalObjectFlags to assign
	 * @param	InOuter				outer for this object
	 * @param	InName				name of the new object
	 */
	UObjectBase( UClass* InClass, EObjectFlags InFlags, EInternalObjectFlags InInternalFlags, UObject *InOuter, FName InName );

	/**
	 * Final destructor, removes the object from the object array, and indirectly, from any annotations
	 **/
	virtual ~UObjectBase();

	/**
	 * Emit GC tokens for UObjectBase, this might be UObject::StaticClass or Default__Class
	 **/
	static void EmitBaseReferences(UClass *RootClass);

protected:
	/**
	 * Just change the FName and Outer and rehash into name hash tables. For use by higher level rename functions.
	 *
	 * @param NewName	new name for this object
	 * @param NewOuter	new outer for this object, if NULL, outer will be unchanged
	 */
	void LowLevelRename(FName NewName,UObject *NewOuter = NULL);

	/** Force any base classes to be registered first */
	virtual void RegisterDependencies() {}

	/** Enqueue the registration for this object. */
	void Register(const TCHAR* PackageName,const TCHAR* Name);
	
	/**
	 * Convert a boot-strap registered class into a real one, add to uobject array, etc
	 *
	 * @param UClassStaticClass Now that it is known, fill in UClass::StaticClass() as the class
	 */
	virtual void DeferredRegister(UClass *UClassStaticClass,const TCHAR* PackageName,const TCHAR* Name);

private:
	/**
	 * Add a newly created object to the name hash tables and the object array
	 *
	 * @param Name name to assign to this uobject
	 * @param InSetInternalFlags Internal object flags to be set on the object once it's been added to the array
	 */
	void AddObject(FName Name, EInternalObjectFlags InSetInternalFlags);
public:
	/**
	 * Checks to see if the object appears to be valid
	 * @return true if this appears to be a valid object
	 */
	bool IsValidLowLevel() const;

	/**
	 * Faster version of IsValidLowLevel.
	 * Checks to see if the object appears to be valid by checking pointers and their alignment.
	 * Name and InternalIndex checks are less accurate than IsValidLowLevel.
	 * @param bRecursive true if the Class pointer should be checked with IsValidLowLevelFast
	 * @return true if this appears to be a valid object
	 */
	bool IsValidLowLevelFast(bool bRecursive = true) const;

	/** 
	 * Returns the unique ID of the object...these are reused so it is only unique while the object is alive.
	 * Useful as a tag.
	**/
	FORCEINLINE uint32 GetUniqueID() const
	{
		return (uint32)InternalIndex;
	}
	FORCEINLINE UClass* GetClass() const
	{
		return ClassPrivate;
	}
	FORCEINLINE UObject* GetOuter() const
	{
		return OuterPrivate;
	}
	FORCEINLINE FName GetFName() const
	{
		return NamePrivate;
	}

	/** 
	 * Returns the stat ID of the object...
	**/
	FORCEINLINE TStatId GetStatID(bool bForDeferredUse = false) const
	{
#if STATS
		// this is done to avoid even registering stats for a disabled group (unless we plan on using it later)
		if (bForDeferredUse || FThreadStats::IsCollectingData(GET_STATID(STAT_UObjectsStatGroupTester)))
		{
			if (!StatID.IsValidStat())
			{
				CreateStatID();
			}
			return StatID;
		}
#endif
		return TStatId(); // not doing stats at the moment, or ever
	}

	
private:

	/** 
	 * Creates this stat ID for the object...and handle a null this pointer
	**/
#if STATS
	void CreateStatID() const;
#endif

protected:
	/**
	 * Set the object flags directly
	 *
	 **/
	FORCEINLINE void SetFlagsTo( EObjectFlags NewFlags )
	{
		checkfSlow((NewFlags & ~RF_AllFlags) == 0, TEXT("%s flagged as 0x%x but is trying to set flags to RF_AllFlags"), *GetFName().ToString(), (int)ObjectFlags);
		ObjectFlags = NewFlags;
	}
public:
	/**
	 * Retrieve the object flags directly
	 *
	 * @return Flags for this object
	 **/
	FORCEINLINE EObjectFlags GetFlags() const
	{
		checkfSlow((ObjectFlags & ~RF_AllFlags) == 0, TEXT("%s flagged as RF_AllFlags"), *GetFName().ToString());
		return ObjectFlags;
	}

	/**
	 *	Atomically adds the specified flags.
	 *	Do not use unless you know what you are doing.
	 *	Designed to be used only by parallel GC and UObject loading thread.
	 */
	FORCENOINLINE void AtomicallySetFlags( EObjectFlags FlagsToAdd )
	{
		int32 OldFlags = 0;
		int32 NewFlags = 0;
		do 
		{
			OldFlags = ObjectFlags;
			NewFlags = OldFlags | FlagsToAdd;
		}
		while( FPlatformAtomics::InterlockedCompareExchange( (int32*)&ObjectFlags, NewFlags, OldFlags) != OldFlags );
	}

	/**
	 *	Atomically clears the specified flags.
	 *	Do not use unless you know what you are doing.
	 *	Designed to be used only by parallel GC and UObject loading thread.
	 */
	FORCENOINLINE void AtomicallyClearFlags( EObjectFlags FlagsToClear )
	{
		int32 OldFlags = 0;
		int32 NewFlags = 0;
		do 
		{
			OldFlags = ObjectFlags;
			NewFlags = OldFlags & ~FlagsToClear;
		}
		while( FPlatformAtomics::InterlockedCompareExchange( (int32*)&ObjectFlags, NewFlags, OldFlags) != OldFlags );
	}

private:

	/** Flags used to track and report various object states. This needs to be 8 byte aligned on 32-bit
	    platforms to reduce memory waste */
	EObjectFlags					ObjectFlags;

	/** Index into GObjectArray...very private. */
	int32								InternalIndex;

	/** Class the object belongs to. */
	UClass*							ClassPrivate;

	/** Name of this object */
	FName							NamePrivate;

	/** Object this object resides in. */
	UObject*						OuterPrivate;


	/** Stat id of this object, 0 if nobody asked for it yet */
	STAT(mutable TStatId				StatID;)

	// This is used by the reinstancer to re-class and re-archetype the current instances of a class before recompiling
	friend class FBlueprintCompileReinstancer;
	void SetClass(UClass* NewClass);

#if HACK_HEADER_GENERATOR
	// Required by UHT makefiles for internal data serialization.
	friend struct FObjectBaseArchiveProxy;
#endif // HACK_HEADER_GENERATOR
};

namespace Internal
{
	/** Internal state indicating whether uobject system is initialized. Do not use that directly. Use
		UObjectInitialized() instead. */
	COREUOBJECT_API extern bool GObjInitialized;
}

/**
 * Checks to see if the UObject subsystem is fully bootstrapped and ready to go.
 * If true, then all objects are registered and auto registration of natives is over, forever.
 *
 * @return true if the UObject subsystem is initialized.
 */
FORCEINLINE bool UObjectInitialized() { return Internal::GObjInitialized; }

/**
 * Force a pending registrant to register now instead of in the natural order
 */
COREUOBJECT_API void UObjectForceRegistration(UObjectBase* Object);

/** 
 * Base class for deferred native class registration
 */
struct FFieldCompiledInInfo
{
	FFieldCompiledInInfo(SIZE_T InClassSize, uint32 InCrc)
		: Size(InClassSize)
		, Crc(InCrc)
		, OldClass(nullptr)
		, bHasChanged(false)
	{
	}

	/** Registers the native class (constructs a UClass object) */
	virtual UClass* Register() const = 0;

	/** Return the package the class belongs in */
	virtual const TCHAR* ClassPackage() const = 0;

	/** Size of the class */
	SIZE_T Size;
	/** CRC of the generated code for this class */
	uint32 Crc;
	/** Old UClass object */
	UClass* OldClass;
	/** True if this class has changed after hot-reload (or new class) */
	bool bHasChanged;
};

/**
* Adds a class to deferred registration queue.
*/
COREUOBJECT_API void UClassCompiledInDefer(FFieldCompiledInInfo* Class, const TCHAR* Name, SIZE_T ClassSize, uint32 Crc);

/**
 * Specialized version of the deferred class registration structure.
 */
template <typename TClass>
struct TClassCompiledInDefer : public FFieldCompiledInInfo
{
	TClassCompiledInDefer(const TCHAR* InName, SIZE_T InClassSize, uint32 InCrc)
	: FFieldCompiledInInfo(InClassSize, InCrc)
	{
		UClassCompiledInDefer(this, InName, InClassSize, InCrc);
	}
	virtual UClass* Register() const override
	{
		return TClass::StaticClass();
	}
	virtual const TCHAR* ClassPackage() const override
	{
		return TClass::StaticPackage();
	}
};

/**
 * Stashes the singleton function that builds a compiled in class. Later, this is executed.
 */
COREUOBJECT_API void UObjectCompiledInDefer(class UClass *(*InRegister)(), class UClass *(*InStaticClass)(), const TCHAR* Name, const TCHAR* PackageName, bool bDynamic, const TCHAR* DynamicPathName, void (*InInitSearchableValues)(TMap<FName, FName>&));

struct FCompiledInDefer
{
	FCompiledInDefer(class UClass *(*InRegister)(), class UClass *(*InStaticClass)(), const TCHAR* PackageName, const TCHAR* Name, bool bDynamic, const TCHAR* DynamicPackageName = nullptr, const TCHAR* DynamicPathName = nullptr, void (*InInitSearchableValues)(TMap<FName, FName>&) = nullptr)
	{
		if (bDynamic)
		{
			GetConvertedDynamicPackageNameToTypeName().Add(FName(DynamicPackageName), FName(Name));
		}
		UObjectCompiledInDefer(InRegister, InStaticClass, Name, PackageName, bDynamic, DynamicPathName, InInitSearchableValues);
	}
};

/**
 * Stashes the singleton function that builds a compiled in struct (StaticStruct). Later, this is executed.
 */
COREUOBJECT_API void UObjectCompiledInDeferStruct(class UScriptStruct *(*InRegister)(), const TCHAR* PackageName, const FName ObjectName, bool bDynamic, const TCHAR* DynamicPathName);

struct FCompiledInDeferStruct
{
	FCompiledInDeferStruct(class UScriptStruct *(*InRegister)(), const TCHAR* PackageName, const TCHAR* Name, bool bDynamic, const TCHAR* DynamicPackageName, const TCHAR* DynamicPathName)
	{
		if (bDynamic)
		{
			GetConvertedDynamicPackageNameToTypeName().Add(FName(DynamicPackageName), FName(Name));
		}
		UObjectCompiledInDeferStruct(InRegister, PackageName, Name, bDynamic, DynamicPathName);
	}
};

/**
 * Either call the passed in singleton, or if this is hot reload, find the existing struct
 */
COREUOBJECT_API class UScriptStruct *GetStaticStruct(class UScriptStruct *(*InRegister)(), UObject* StructOuter, const TCHAR* StructName, SIZE_T Size, uint32 Crc);

/**
 * Stashes the singleton function that builds a compiled in enum. Later, this is executed.
 */
COREUOBJECT_API void UObjectCompiledInDeferEnum(class UEnum *(*InRegister)(), const TCHAR* PackageName, const FName ObjectName, bool bDynamic, const TCHAR* DynamicPathName);

struct FCompiledInDeferEnum
{
	FCompiledInDeferEnum(class UEnum *(*InRegister)(), const TCHAR* PackageName, const TCHAR* Name, bool bDynamic, const TCHAR* DynamicPackageName, const TCHAR* DynamicPathName)
	{
		if (bDynamic)
		{
			GetConvertedDynamicPackageNameToTypeName().Add(FName(DynamicPackageName), FName(Name));
		}
		UObjectCompiledInDeferEnum(InRegister, PackageName, Name, bDynamic, DynamicPathName);
	}
};

/**
 * Either call the passed in singleton, or if this is hot reload, find the existing enum
 */
COREUOBJECT_API class UEnum *GetStaticEnum(class UEnum *(*InRegister)(), UObject* EnumOuter, const TCHAR* EnumName);

COREUOBJECT_API class UScriptStruct* FindExistingStructIfHotReloadOrDynamic(UObject* Outer, const TCHAR* StructName, SIZE_T Size, uint32 Crc, bool bIsDynamic);
COREUOBJECT_API class UEnum* FindExistingEnumIfHotReloadOrDynamic(UObject* Outer, const TCHAR* EnumName, SIZE_T Size, uint32 Crc, bool bIsDynamic);

/** Must be called after a module has been loaded that contains UObject classes */
COREUOBJECT_API void ProcessNewlyLoadedUObjects();

#if WITH_HOT_RELOAD
/** Map of duplicated CDOs for reinstancing during hot-reload purposes. */
COREUOBJECT_API TMap<UObject*, UObject*>& GetDuplicatedCDOMap();
#endif // WITH_HOT_RELOAD

/**
 * Final phase of UObject initialization. all auto register objects are added to the main data structures.
 */
void UObjectBaseInit();

/**
 * Final phase of UObject shutdown
 */
void UObjectBaseShutdown();

