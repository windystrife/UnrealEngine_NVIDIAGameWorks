// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM()	
enum class EHierarchicalLODActionType : uint8
{
	InvalidAction,
	CreateCluster,
	AddActorToCluster,
	MoveActorToCluster,
	RemoveActorFromCluster,
	MergeClusters,
	ChildCluster,

	MAX
};
