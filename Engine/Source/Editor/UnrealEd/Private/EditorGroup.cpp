// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/GroupActor.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "ActorGroupingUtils.h"


void UUnrealEdEngine::edactRegroupFromSelected()
{
	UActorGroupingUtils::Get()->GroupSelected();
}


void UUnrealEdEngine::edactUngroupFromSelected()
{
	UActorGroupingUtils::Get()->UngroupSelected();
}


void UUnrealEdEngine::edactLockSelectedGroups()
{
	UActorGroupingUtils::Get()->LockSelectedGroups();
}


void UUnrealEdEngine::edactUnlockSelectedGroups()
{
	UActorGroupingUtils::Get()->UnlockSelectedGroups();
}


void UUnrealEdEngine::edactAddToGroup()
{
	UActorGroupingUtils::Get()->AddSelectedToGroup();
}


void UUnrealEdEngine::edactRemoveFromGroup()
{
	UActorGroupingUtils::Get()->RemoveSelectedFromGroup();
}

