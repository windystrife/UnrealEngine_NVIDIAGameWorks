// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationStateMachineGraph.h"

#define LOCTEXT_NAMESPACE "AnimationGraph"

/////////////////////////////////////////////////////
// UAnimationStateMachineGraph

UAnimationStateMachineGraph::UAnimationStateMachineGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAllowDeletion = false;
	bAllowRenaming = true;
}


#undef LOCTEXT_NAMESPACE
