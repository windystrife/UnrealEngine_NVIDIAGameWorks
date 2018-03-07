// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequenceObjectReference.h"
#include "ControlRig.h"

FControlRigSequenceObjectReference FControlRigSequenceObjectReference::Create(UControlRig* InControlRig)
{
	check(InControlRig);

	FControlRigSequenceObjectReference NewReference;
	NewReference.ControlRigClass = InControlRig->GetClass();

	return NewReference;
}

bool FControlRigSequenceObjectReferenceMap::HasBinding(const FGuid& ObjectId) const
{
	return BindingIds.Contains(ObjectId);
}

void FControlRigSequenceObjectReferenceMap::RemoveBinding(const FGuid& ObjectId)
{
	int32 Index = BindingIds.IndexOfByKey(ObjectId);
	if (Index != INDEX_NONE)
	{
		BindingIds.RemoveAtSwap(Index, 1, false);
		References.RemoveAtSwap(Index, 1, false);
	}
}

void FControlRigSequenceObjectReferenceMap::CreateBinding(const FGuid& ObjectId, const FControlRigSequenceObjectReference& ObjectReference)
{
	int32 ExistingIndex = BindingIds.IndexOfByKey(ObjectId);
	if (ExistingIndex == INDEX_NONE)
	{
		ExistingIndex = BindingIds.Num();

		BindingIds.Add(ObjectId);
		References.AddDefaulted();
	}

	References[ExistingIndex].Array.AddUnique(ObjectReference);
}
