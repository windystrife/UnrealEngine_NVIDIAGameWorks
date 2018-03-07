// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTree/Composites/BTComposite_Sequence.h"

UBTComposite_Sequence::UBTComposite_Sequence(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Sequence";

	OnNextChild.BindUObject(this, &UBTComposite_Sequence::GetNextChildHandler);
}

int32 UBTComposite_Sequence::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const
{
	// failure = quit
	int32 NextChildIdx = BTSpecialChild::ReturnToParent;

	if (PrevChild == BTSpecialChild::NotInitialized)
	{
		// newly activated: start from first
		NextChildIdx = 0;
	}
	else if (LastResult == EBTNodeResult::Succeeded && (PrevChild + 1) < GetChildrenNum())
	{
		// success = choose next child
		NextChildIdx = PrevChild + 1;
	}

	return NextChildIdx;
}

#if WITH_EDITOR

bool UBTComposite_Sequence::CanAbortLowerPriority() const
{
	// don't allow aborting lower priorities, as it breaks sequence order and doesn't makes sense
	return false;
}

FName UBTComposite_Sequence::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Composite.Sequence.Icon");
}

#endif
