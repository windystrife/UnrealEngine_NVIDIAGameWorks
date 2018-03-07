// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/Guid.h"
#include "Engine/Engine.h"
#include "Paths.h"
#include "LevelSequenceBindingReference.generated.h"

/**
 * An external reference to an level sequence object, resolvable through an arbitrary context.
 * 
 * Bindings consist of an optional package name, and the path to the object within that package.
 * Where package name is empty, the reference is a relative path from a specific outer (the context).
 * Currently, the package name should only ever be empty for component references, which must remain relative bindings to work correctly with spawnables and reinstanced actors.
 */
USTRUCT()
struct FLevelSequenceBindingReference
{
	GENERATED_BODY();

	/**
	 * Default construction only used for serialization
	 */
	FLevelSequenceBindingReference() {}

	/**
	 * Construct a new binding reference from an object, and a given context (expected to be either a UWorld, or an AActor)
	 */
	LEVELSEQUENCE_API FLevelSequenceBindingReference(UObject* InObject, UObject* InContext);

	/**
	 * Resolve this reference within the specified context
	 *
	 * @param InContext		The context to resolve the binding within. Either a UWorld or an AActor where this binding relates to an actor component
	 * @return The object (usually an Actor or an ActorComponent).
	 */
	LEVELSEQUENCE_API UObject* Resolve(UObject* InContext) const;

	/** Handles ExternalObjectPath fixup */
	void PostSerialize(const FArchive& Ar);

private:

	/** Replaced by ExternalObjectPath */
	UPROPERTY()
	FString PackageName_DEPRECATED;

	/** Path to a specific actor/component inside an external package */
	UPROPERTY()
	FSoftObjectPath ExternalObjectPath;

	/** Object path relative to a passed in context object, this is used if ExternalObjectPath is invalid */
	UPROPERTY()
	FString ObjectPath;
};


template<>
struct TStructOpsTypeTraits<FLevelSequenceBindingReference> : public TStructOpsTypeTraitsBase2<FLevelSequenceBindingReference>
{
	enum
	{
		WithPostSerialize = true,
	};
};

/**
 * An array of binding references
 */
USTRUCT()
struct FLevelSequenceBindingReferenceArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FLevelSequenceBindingReference> References;
};


/**
 * Structure that stores a one to many mapping from object binding ID, to object references that pertain to that ID.
 */
USTRUCT()
struct FLevelSequenceBindingReferences
{
	GENERATED_BODY()

	/**
	 * Check whether this map has a binding for the specified object id
	 * @return true if this map contains a binding for the id, false otherwise
	 */
	bool HasBinding(const FGuid& ObjectId) const;

	/**
	 * Remove a binding for the specified ID
	 *
	 * @param ObjectId	The ID to remove
	 */
	void RemoveBinding(const FGuid& ObjectId);

	/**
	 * Add a binding for the specified ID
	 *
	 * @param ObjectId	The ID to associate the object with
	 * @param InObject	The object to associate
	 * @param InContext	A context in which InObject resides (either a UWorld, or an AActor)
	 */
	void AddBinding(const FGuid& ObjectId, UObject* InObject, UObject* InContext);

	/**
	 * Resolve a binding for the specified ID using a given context
	 *
	 * @param ObjectId		The ID to associate the object with
	 * @param InContext		A context in which InObject resides
	 * @param OutObjects	Array to populate with resolved object bindings
	 */
	void ResolveBinding(const FGuid& ObjectId, UObject* InContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const;

private:

	/** The map from object binding ID to an array of references that pertain to that ID */
	UPROPERTY()
	TMap<FGuid, FLevelSequenceBindingReferenceArray> BindingIdToReferences;
};