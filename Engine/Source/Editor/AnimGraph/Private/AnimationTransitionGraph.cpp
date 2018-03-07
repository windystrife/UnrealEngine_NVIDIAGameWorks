// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationTransitionGraph.h"

#define LOCTEXT_NAMESPACE "AnimationStateGraph"

/////////////////////////////////////////////////////
// UAnimationTransitionGraph

UAnimationTransitionGraph::UAnimationTransitionGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimGraphNode_TransitionResult* UAnimationTransitionGraph::GetResultNode()
{
	return MyResultNode;
}

#undef LOCTEXT_NAMESPACE
