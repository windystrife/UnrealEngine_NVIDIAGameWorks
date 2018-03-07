// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LazyObjectPtr.h"
#include "ControlRig.h"
#include "ControlRigSequenceObjectReference.generated.h"


/**
 * An external reference to an level sequence object, resolvable through an arbitrary context.
 */
USTRUCT()
struct FControlRigSequenceObjectReference
{
	GENERATED_BODY()

	/**
	 * Default construction to a null reference
	 */
	FControlRigSequenceObjectReference() {}

	/**
	 * Generates a new reference to an object within a given context.
	 *
	 * @param InControlRig The ControlRig to create a reference for
	 */
	CONTROLRIG_API static FControlRigSequenceObjectReference Create(UControlRig* InControlRig);

	/**
	 * Check whether this object reference is valid or not
	 */
	bool IsValid() const
	{
		return ControlRigClass.Get() != nullptr;
	}

	/**
	 * Equality comparator
	 */
	friend bool operator==(const FControlRigSequenceObjectReference& A, const FControlRigSequenceObjectReference& B)
	{
		return A.ControlRigClass == B.ControlRigClass;
	}

private:

	/** The type of this animation ControlRig */
	UPROPERTY()
	TSubclassOf<UControlRig> ControlRigClass;
};

USTRUCT()
struct FControlRigSequenceObjectReferences
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FControlRigSequenceObjectReference> Array;
};

USTRUCT()
struct FControlRigSequenceObjectReferenceMap
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
	 * Create a binding for the specified ID
	 *
	 * @param ObjectId				The ID to associate the component with
	 * @param ObjectReference	The component reference to bind
	 */
	void CreateBinding(const FGuid& ObjectId, const FControlRigSequenceObjectReference& ObjectReference);

private:
	
	UPROPERTY()
	TArray<FGuid> BindingIds;

	UPROPERTY()
	TArray<FControlRigSequenceObjectReferences> References;
};