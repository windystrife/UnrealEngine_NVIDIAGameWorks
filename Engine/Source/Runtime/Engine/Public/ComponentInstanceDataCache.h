// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class AActor;
class UActorComponent;
class USceneComponent;
enum class EComponentCreationMethod : uint8;

/** At what point in the rerun construction script process is ApplyToActor being called for */
enum class ECacheApplyPhase
{
	PostSimpleConstructionScript,	// After the simple construction script has been run
	PostUserConstructionScript,		// After the user construction script has been run
};

/** Base class for component instance cached data of a particular type. */
class ENGINE_API FActorComponentInstanceData
{
public:
	FActorComponentInstanceData();
	FActorComponentInstanceData(const UActorComponent* SourceComponent);

	virtual ~FActorComponentInstanceData()
	{}

	/** Determines whether this component instance data matches the component */
	bool MatchesComponent(const UActorComponent* Component, const UObject* ComponentTemplate, const TMap<UActorComponent*, const UObject*>& ComponentToArchetypeMap) const;

	/** Applies this component instance data to the supplied component */
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase);

	/** Replaces any references to old instances during Actor reinstancing */
	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) { };

	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	bool ContainsSavedProperties() const { return SavedProperties.Num() > 0; }

	const UClass* GetComponentClass() const { return SourceComponentTemplate ? SourceComponentTemplate->GetClass() : nullptr; }

protected:
	/** The template used to create the source component */
	const UObject* SourceComponentTemplate;

	/** The index of the source component in its owner's serialized array 
		when filtered to just that component type */
	int32 SourceComponentTypeSerializedIndex;

	/** The method that was used to create the source component */
	EComponentCreationMethod SourceComponentCreationMethod;

	TArray<uint8> SavedProperties;
	TArray<UObject*> InstancedObjects;
};

/** 
 *	Cache for component instance data.
 *	Note, does not collect references for GC, so is not safe to GC if the cache is only reference to a UObject.
 */
class ENGINE_API FComponentInstanceDataCache
{
public:

	FComponentInstanceDataCache() {}

	/** Constructor that also populates cache from Actor */
	FComponentInstanceDataCache(const AActor* InActor);

	~FComponentInstanceDataCache();

	/** Iterates over an Actor's components and applies the stored component instance data to each */
	void ApplyToActor(AActor* Actor, const ECacheApplyPhase CacheApplyPhase) const;

	/** Iterates over components and replaces any object references with the reinstanced information */
	void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	bool HasInstanceData() const { return ComponentsInstanceData.Num() > 0; }

	void AddReferencedObjects(FReferenceCollector& Collector);

private:
	/** Map of data type name to data of that type */
	TArray< FActorComponentInstanceData* > ComponentsInstanceData;

	TMap< USceneComponent*, FTransform > InstanceComponentTransformToRootMap;
};
