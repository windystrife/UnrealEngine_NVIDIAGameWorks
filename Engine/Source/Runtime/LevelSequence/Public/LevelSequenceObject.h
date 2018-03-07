// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/LazyObjectPtr.h"
#include "LevelSequenceObject.generated.h"


/**
 * Structure for animated Actor objects.
 */
USTRUCT()
struct FLevelSequenceObject
{
	GENERATED_BODY()

public:

	/** Creates and initializes a new instance. */
	FLevelSequenceObject()
		: ObjectOrOwner(nullptr)
		, CachedComponent(nullptr)
	{ }

	/**
	 * Creates and initializes a new instance from an object.
	 *
	 * @param InObject The object to be bound.
	 */
	FLevelSequenceObject(UObject* InObject)
		: ObjectOrOwner(InObject)
		, CachedComponent(nullptr)
	{ }

	/**
	 * Creates and initializes a new instance from an object component.
	 *
	 * @param InOwner The object that owns the component.
	 * @param InComponentName The component to be bound.
	 */
	FLevelSequenceObject(UObject* InOwner, FString InComponentName)
		: ObjectOrOwner(InOwner)
		, ComponentName(InComponentName)
		, CachedComponent(nullptr)
	{ }

	/**
	 * Compares two bindings for equality.
	 *
	 * @param X The first binding to compare.
	 * @param Y The second binding to compare.
	 * @return true if the bindings refer to the same object, false otherwise.
	 */
	friend bool operator==(const FLevelSequenceObject& X, const FLevelSequenceObject& Y)
	{
		return (X.ObjectOrOwner == Y.ObjectOrOwner) && (X.ComponentName == Y.ComponentName);
	}

	/**
	 * Gets a pointer to the possessed object.
	 *
	 * @return The object (usually an Actor or an ActorComponent).
	 */
	LEVELSEQUENCE_API UObject* GetObject() const;

private:

	/** The object or the owner of the object being possessed. */
	UPROPERTY()
	TLazyObjectPtr<UObject> ObjectOrOwner;

	/** Optional name of an ActorComponent. */
	UPROPERTY()
	FString ComponentName;

	/** Cached pointer to the Actor component (only if ComponentName is set). */
	UPROPERTY(transient)
	mutable TWeakObjectPtr<UObject> CachedComponent;
};
