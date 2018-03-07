// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSingleton.h"
#include "UObject/UObjectGlobals.h"

class UDynamicClass;
struct FCompilerNativizationOptions;
class ITargetPlatform;

struct FBlueprintWarningDeclaration
{
	FBlueprintWarningDeclaration(FName InWarningIdentifier, FText InWarningDescription)
		: WarningIdentifier(InWarningIdentifier)
		, WarningDescription( InWarningDescription )
	{
	}

	FName WarningIdentifier;
	FText WarningDescription;
};

typedef void (*FFlushReinstancingQueueFPtr)();

/** 
 * This set of functions contains blueprint related UObject functionality.
 */
struct FBlueprintSupport
{
	/** 
	 * Defined in BlueprintSupport.cpp
	 * Duplicates all fields of a struct in depth-first order. It makes sure that everything contained
	 * in a class is duplicated before the struct itself, as well as all function parameters before the
	 * function itself.
	 *
	 * @param	StructToDuplicate			Instance of the struct that is about to be duplicated
	 * @param	Writer						duplicate writer instance to write the duplicated data to
	 */
	static void DuplicateAllFields(class UStruct* StructToDuplicate, class FDuplicateDataWriter& Writer);

	/** 
	 * A series of query functions that we can use to easily gate-off/disable 
	 * aspects of the deferred loading (mostly for testing purposes). 
	 */
	static bool UseDeferredDependencyLoading();
	static bool IsDeferredExportCreationDisabled();
	static bool IsDeferredCDOInitializationDisabled();

	/** Initializes the BP Compilation Manager if it's enabled: */
	COREUOBJECT_API static void InitializeCompilationManager();

	/** Checks for any old instances and reinstances them: */
	static void FlushReinstancingQueue();
	COREUOBJECT_API static void SetFlushReinstancingQueueFPtr(FFlushReinstancingQueueFPtr Ptr);

	/** Tells if the specified object is one of the many flavors of FLinkerPlaceholderBase that we have. */
	COREUOBJECT_API static bool IsDeferredDependencyPlaceholder(UObject* LoadedObj);

	/** Not a particularly fast function. Mostly intended for validation in debug builds. */
	static bool IsInBlueprintPackage(UObject* LoadedObj);

	COREUOBJECT_API static void RegisterBlueprintWarning(const FBlueprintWarningDeclaration& Warning);
	COREUOBJECT_API static const TArray<FBlueprintWarningDeclaration>& GetBlueprintWarnings();
	COREUOBJECT_API static void UpdateWarningBehavior(const TArray<FName>& WarningIdentifiersToTreatAsError, const TArray<FName>& WarningIdentifiersToSuppress);
	COREUOBJECT_API static bool ShouldTreatWarningAsError(FName WarningIdentifier);
	COREUOBJECT_API static bool ShouldSuppressWarning(FName WarningIdentifier);

#if WITH_EDITOR
	/** Function that walks the object graph, ensuring that there are no references to TRASH or REINST classes: */
	COREUOBJECT_API static void ValidateNoRefsToOutOfDateClasses();

	/** Function that walks the object graph, ensuring that there are no references to SKEL classes: */
	COREUOBJECT_API static void ValidateNoExternalRefsToSkeletons();
#endif
};

#if WITH_EDITOR
/**
 * This is a helper struct that allows us to gather all previously unloaded class dependencies of a UClass
 * The first time we create a new UClass object in FLinkerLoad::CreateExport(), we register it as a dependency
 * master.  Any subsequent UClasses that are created for the first time during the preload of that class are
 * added to the list as potential cyclic referencers.  We then step over the list at the end of the load, and
 * recompile any classes that may depend on each other a second time to ensure that that functions and properties
 * are properly resolved
 */
struct COREUOBJECT_API FScopedClassDependencyGather
{
public:
	FScopedClassDependencyGather(UClass* ClassToGather);
	~FScopedClassDependencyGather();

	/**
	 * Post load, some systems would like an easy list of dependencies. This will
	 * retrieve that latest BatchClassDependencies (filled with dependencies from
	 * the last loaded class).
	 * 
	 * @return The most recent array of tracked dependencies.
	 */
	static TArray<UClass*> const& GetCachedDependencies();

private:
	/** Whether or not this dependency gather is the dependency master, and thus should process all dependencies in the destructor */
	bool bMasterClass;	

	/** The current class that is gathering potential dependencies in this scope */
	static UClass* BatchMasterClass;

	/** List of dependencies (i.e. UClasses that have been newly instantiated) in the scope of this dependency gather */
	static TArray<UClass*> BatchClassDependencies;

	FScopedClassDependencyGather();
};

enum class EReplacementResult
{
	/** Don't replace the provided package at all */
	DontReplace,

	/** Generate a stub file, but don't replace the package */
	GenerateStub,

	/** Completely replace the file with generated code */
	ReplaceCompletely
};

struct COREUOBJECT_API FScopedPlaceholderRawContainerTracker
{
public:
	FScopedPlaceholderRawContainerTracker(void* InData);
	~FScopedPlaceholderRawContainerTracker();

private:
	void* Data;
};

/**
 * Interface needed by CoreUObject to the BlueprintNativeCodeGen logic. Used by cooker to convert assets 
 * to native code.
 */
struct IBlueprintNativeCodeGenCore
{
	/** Returns the current IBlueprintNativeCodeGenCore, may return nullptr */
	COREUOBJECT_API static const IBlueprintNativeCodeGenCore* Get();

	/**
	 * Registers the IBlueprintNativeCodeGenCore, just used to point us at an implementation.
	 * By default, there is no IBlueprintNativeCodeGenCore, and thus no blueprints are
	 * replaced at cook.
	 */
	COREUOBJECT_API static void Register(const IBlueprintNativeCodeGenCore* Coordinator);

	/**
	 * Determines whether the provided package needs to be replaced (in part or completely)
	 * 
	 * @param Package	The package in question
	 * @return Whether the package should be converted
	 */
	virtual EReplacementResult IsTargetedForReplacement(const UPackage* Package, const FCompilerNativizationOptions& NativizationOptions) const = 0;
	
	/**
	 * Determines whether the provided object needs to be replaced (in part or completely).
	 * Some objects in a package may require conversion and some may not. If any object 
	 * in a package wants to be converted then it is implied that all other objects will 
	 * be converted with it (no support for partial package conversion, beyond stubs)
	 *
	 * @param Object	The package in question
	 * @return Whether the object should be converted
	 */
	virtual EReplacementResult IsTargetedForReplacement(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const = 0;

	/** 
	 * Function used to change the type of a class from, say, UBlueprintGeneratedClass to 
	 * UDynamicClass. Cooking (and conversion in general) must be order independent so
	 * The scope of this kind of type swap is limited.
	 * 
	 * @param Object whose class will be replaced
	 * @return A replacement class ptr, null if none
	 */
	virtual UClass* FindReplacedClassForObject(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const = 0;
	
	/** 
	 * Function used to change the path of subobject from a nativized class.
	 * 
	 * @param Object Imported Object.
	 * @param OutName Referenced to name, that will be saved in import table.
	 * @return An Outer object that should be saved in import table.
	 */
	virtual UObject* FindReplacedNameAndOuter(UObject* Object, FName& OutName, const FCompilerNativizationOptions& NativizationOptions) const = 0;

	/*
	 * Return nativization options for given platform.
	 */
	virtual const FCompilerNativizationOptions& GetNativizationOptionsForPlatform(const ITargetPlatform* Platform) const = 0;
};

#endif // WITH_EDITOR

/** 
 * A helper struct for storing FObjectInitializers that were not run on 
 * Blueprint CDO's (or CDO sub-objects) post-construction (presumably because 
 * that CDO's super had not been fully serialized yet). 
 * 
 * This was designed to hold onto FObjectInitializers until a later point, when 
 * they can properly be ran (presumably in FLinkerLoad::ResolveDeferredExports,
 * after the super has been finalized).
 */
struct FDeferredObjInitializerTracker : TThreadSingleton<FDeferredObjInitializerTracker>
{
public:
	FDeferredObjInitializerTracker() : ResolvingClass(nullptr) {}

	/** Stores a copy of the specified FObjectInitializer and returns a pointer to it (could be null if a corresponding class could not be determined). */
	static FObjectInitializer* Add(const FObjectInitializer& DeferringInitializer);

	/** Looks up a FObjectInitializer that was deferred for the specified class (an FObjectInitializer for that class's CDO). */
	static FObjectInitializer* Find(UClass* LoadClass);

	/** Checks to see if the specified class has been logged as deferred (meaning its CDO hasn't had FObjectInitializer::PostConstructInit() ran on it yet). */
	static bool IsCdoDeferred(UClass* LoadClass);

	/** Determines if the specified sub-object should have its Preload() skipped; if so, this will cache the sub-object and return true. */
	static bool DeferSubObjectPreload(UObject* SubObject);

	/** Destroys any FObjectInitializers that were cached corresponding to the specified class. */
	static void Remove(UClass* LoadClass);

	/** Runs FObjectInitializer::PostConstructInit() on the specified class's CDO (if it was deferred), and preloads any sub-objects that were skipped. */
	static bool ResolveDeferredInitialization(UClass* LoadClass);

	/**  */
	static void ResolveDeferredSubObjects(UObject* CDO);

	/**  */
	static void ResolveDeferredSubClassObjects(UClass* SuperClass);

private:
	/** A map that tracks the relationship between Blueprint classes and FObjectInitializers for their CDOs */
	TMap<UClass*, FObjectInitializer> DeferredInitializers;
	/** Track default sub-objects that had their Preload() skipped, because the owning CDO's initialization should happen first */
	TMultiMap<UClass*, UObject*> DeferredSubObjects;
	/** Used to keep ResolveDeferredSubObjects() from re-adding sub-objects via DeferSubObjectPreload() */
	UClass* ResolvingClass;
	/** Tracks sub-classes that have had their CDO deferred as a result of the super not being fully serialized */
	TMultiMap<UClass*, UClass*> SuperClassMap;
	/** A map that tracks deferred FObjectInitializers for a class/CDO's sub-objects (not DSOs, but things like component override templates)*/
	TMultiMap<UClass*, FObjectInitializer> DeferredSubObjInitializers;
};

struct FBlueprintDependencyType
{
	uint8 bSerializationBeforeSerializationDependency : 1;
	uint8 bCreateBeforeSerializationDependency : 1;
	uint8 bSerializationBeforeCreateDependency : 1;
	uint8 bCreateBeforeCreateDependency : 1;

	FBlueprintDependencyType()
		: bSerializationBeforeSerializationDependency(0)
		, bCreateBeforeSerializationDependency(0)
		, bSerializationBeforeCreateDependency(0)
		, bCreateBeforeCreateDependency(0) {}

	FBlueprintDependencyType(bool bInSerializationBeforeSerializationDependency
		, bool bInCreateBeforeSerializationDependency
		, bool bInSerializationBeforeCreateDependency
		, bool bInCreateBeforeCreateDependency)
		: bSerializationBeforeSerializationDependency(bInSerializationBeforeSerializationDependency)
		, bCreateBeforeSerializationDependency(bInCreateBeforeSerializationDependency)
		, bSerializationBeforeCreateDependency(bInSerializationBeforeCreateDependency)
		, bCreateBeforeCreateDependency(bInCreateBeforeCreateDependency)
	{}
};

struct COREUOBJECT_API FCompactBlueprintDependencyData
{
	int16 ObjectRefIndex;
	FBlueprintDependencyType StructDependency;
	FBlueprintDependencyType CDODependency;

	FCompactBlueprintDependencyData()
		: ObjectRefIndex(-1)
	{}

	FCompactBlueprintDependencyData(int16 InObjectRefIndex
		, FBlueprintDependencyType InStructDependency
		, FBlueprintDependencyType InCDODependency = FBlueprintDependencyType())
		: ObjectRefIndex(InObjectRefIndex)
		, StructDependency(InStructDependency)
		, CDODependency(InCDODependency)
	{}
};

struct COREUOBJECT_API FBlueprintDependencyObjectRef
{
	FName PackageName;
	FName ObjectName;
	FName ClassPackageName;
	FName ClassName;

	FBlueprintDependencyObjectRef() {}

	FORCENOINLINE FBlueprintDependencyObjectRef(const TCHAR* InPackageFolder
		, const TCHAR* InShortPackageName
		, const TCHAR* InObjectName
		, const TCHAR* InClassPackageName
		, const TCHAR* InClassName);
};

struct COREUOBJECT_API FBlueprintDependencyData
{
	FBlueprintDependencyObjectRef ObjectRef;
	// 0 - dependency type for dynamic class or UDS
	// 1 - dependency type for CD0
	FBlueprintDependencyType DependencyTypes[2];

	int16 ObjectRefIndex; // NativizationWithoutEDLBT

	FBlueprintDependencyData(const FBlueprintDependencyObjectRef& InObjectRef
		, const FCompactBlueprintDependencyData& InCompactDependencyData)
		: ObjectRef(InObjectRef)
		, ObjectRefIndex(InCompactDependencyData.ObjectRefIndex)
	{
		DependencyTypes[0] = InCompactDependencyData.StructDependency;
		DependencyTypes[1] = InCompactDependencyData.CDODependency;
	}

	bool operator==(const FBlueprintDependencyData& Other) const
	{
		return Other.ObjectRefIndex == ObjectRefIndex;
	}

	static bool ContainsDependencyData(TArray<FBlueprintDependencyData>& Assets, int16 ObjectRefIndex);
	static void AppendUniquely(TArray<FBlueprintDependencyData>& Destination, const TArray<FBlueprintDependencyData>& AdditionalData);
};

/**
 *	Stores info about dependencies of native classes converted from BPs
 */
struct COREUOBJECT_API FConvertedBlueprintsDependencies
{
	typedef void(*GetDependenciesNamesFunc)(TArray<FBlueprintDependencyData>&);

private:

	TMap<FName, GetDependenciesNamesFunc> PackageNameToGetter;

public:
	static FConvertedBlueprintsDependencies& Get();

	void RegisterConvertedClass(FName PackageName, GetDependenciesNamesFunc GetAssets);

	/** Get all assets paths necessary for the class with the given class name and all converted classes that dependencies. */
	void GetAssets(FName PackageName, TArray<FBlueprintDependencyData>& OutDependencies) const;

	static void FillUsedAssetsInDynamicClass(UDynamicClass* DynamicClass, GetDependenciesNamesFunc GetUsedAssets);
};
