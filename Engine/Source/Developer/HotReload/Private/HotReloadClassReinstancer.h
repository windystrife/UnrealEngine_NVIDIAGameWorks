// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#if WITH_ENGINE
#include "Kismet2/KismetReinstanceUtilities.h"
#endif

class UBlueprint;

#if WITH_ENGINE


/**
 * Helper class used for re-instancing native and blueprint classes after hot-reload
 */
class FHotReloadClassReinstancer : public FBlueprintCompileReinstancer
{
	/** Holds a property and its offset in the serialized properties data array */
	struct FCDOProperty
	{
		FCDOProperty()
			: Property(nullptr)
			, SubobjectName(NAME_None)
			, SerializedValueOffset(0)
			, SerializedValueSize(0)
		{}

		UProperty* Property;
		FName SubobjectName;
		int64 SerializedValueOffset;
		int64 SerializedValueSize;
	};

	/** Contains all serialized CDO property data and the map of all serialized properties */
	struct FCDOPropertyData
	{
		TArray<uint8> Bytes;
		TMap<FName, FCDOProperty> Properties;
	};

	/** Hot-reloaded version of the old class */
	UClass* NewClass;

	/** Serialized properties of the original CDO (before hot-reload) */
	FCDOPropertyData OriginalCDOProperties;

	/** Serialized properties of the new CDO (after hot-reload) */
	FCDOPropertyData ReconstructedCDOProperties;

	/** True if the provided native class needs re-instancing */
	bool bNeedsReinstancing;	

	/** Necessary for delta serialization */
	UObject* CopyOfPreviousCDO;

	/** 
	 * Sets the re-instancer up for new class re-instancing 
	 *
	 * @param InNewClass Class that has changed after hot-reload
	 * @param InOldClass Class before it was hot-reloaded
	 */
	void SetupNewClassReinstancing(UClass* InNewClass, UClass* InOldClass);

	/**
	* Sets the re-instancer up for old class re-instancing. Always re-creates the CDO.
	*
	* @param InOldClass Class that has NOT changed after hot-reload
	*/
	void RecreateCDOAndSetupOldClassReinstancing(UClass* InOldClass);
	
	/**
	* Creates a mem-comparable array of data containing CDO property values.
	*
	* @param InObject CDO
	* @param OutData Data containing all of the CDO property values
	*/
	void SerializeCDOProperties(UObject* InObject, FCDOPropertyData& OutData);

	/**
	 * Re-creates class default object.
	 *
	 * @param InClass Class that has NOT changed after hot-reload.
	 * @param InOuter Outer for the new CDO.
	 * @param InName Name of the new CDO.
	 * @param InFlags Flags of the new CDO.
	 */
	void ReconstructClassDefaultObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags InFlags);

	/** Updates property values on instances of the hot-reloaded class */
	void UpdateDefaultProperties();

	/** Returns true if the properties of the CDO have changed during hot-reload */
	FORCEINLINE bool DefaultPropertiesHaveChanged() const
	{
		return OriginalCDOProperties.Bytes.Num() != ReconstructedCDOProperties.Bytes.Num() ||
			FMemory::Memcmp(OriginalCDOProperties.Bytes.GetData(), ReconstructedCDOProperties.Bytes.GetData(), OriginalCDOProperties.Bytes.Num());
	}

public:

	/** Sets the re-instancer up to re-instance native classes */
	FHotReloadClassReinstancer(UClass* InNewClass, UClass* InOldClass, const TMap<UClass*, UClass*>& OldToNewClassesMap, TMap<UObject*, UObject*>& OutReconstructedCDOsMap, TSet<UBlueprint*>& InBPSetToRecompile, TSet<UBlueprint*>& InBPSetToRecompileBytecodeOnly);
	
	/** Destructor */
	virtual ~FHotReloadClassReinstancer();

	/** If true, the class needs re-instancing */
	FORCEINLINE bool ClassNeedsReinstancing() const
	{
		return bNeedsReinstancing;
	}

	/** Reinstances all objects of the hot-reloaded class and update their properties to match the new CDO */
	void ReinstanceObjectsAndUpdateDefaults();

	/** Creates the reinstancer as a sharable object */
	static TSharedPtr<FHotReloadClassReinstancer> Create(UClass* InNewClass, UClass* InOldClass, const TMap<UClass*, UClass*>& OldToNewClassesMap, TMap<UObject*, UObject*>& OutReconstructedCDOsMap, TSet<UBlueprint*>& InBPSetToRecompile, TSet<UBlueprint*>& InBPSetToRecompileBytecodeOnly)
	{
		return MakeShareable(new FHotReloadClassReinstancer(InNewClass, InOldClass, OldToNewClassesMap, OutReconstructedCDOsMap, InBPSetToRecompile, InBPSetToRecompileBytecodeOnly));
	}

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface

	virtual bool IsClassObjectReplaced() const override  { return true; }

	virtual void EnlistDependentBlueprintToRecompile(UBlueprint* BP, bool bBytecodeOnly) override;
	virtual void BlueprintWasRecompiled(UBlueprint* BP, bool bBytecodeOnly) override;

protected:

	// FBlueprintCompileReinstancer interface
	virtual bool ShouldPreserveRootComponentOfReinstancedActor() const override { return false; }
	// End of FBlueprintCompileReinstancer interface

private:
	/** Reference to reconstructed CDOs map in this hot-reload session. */
	TMap<UObject*, UObject*>& ReconstructedCDOsMap;
	TSet<UBlueprint*>& BPSetToRecompile;
	TSet<UBlueprint*>& BPSetToRecompileBytecodeOnly;
	const TMap<UClass*, UClass*>& OldToNewClassesMap;
};

#endif // WITH_ENGINE
