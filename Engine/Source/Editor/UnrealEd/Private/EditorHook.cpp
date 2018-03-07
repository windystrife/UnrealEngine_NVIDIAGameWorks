// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


// Includes.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Actor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Selection.h"
#include "EditorModeManager.h"
#include "EdMode.h"
#include "LevelEditor.h"


uint32			EngineThreadId;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

extern int GLastScroll;

// Misc.
UEngine* Engine;

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::NotifyPreChange(UProperty* PropertyAboutToChange)
{
}
void UUnrealEdEngine::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	// Notify all active modes of actor property changes.
	TArray<FEdMode*> ActiveModes;
	GLevelEditorModeTools().GetActiveModes(ActiveModes);

	for (int32 ModeIndex = 0; ModeIndex < ActiveModes.Num(); ++ModeIndex)
	{
		ActiveModes[ModeIndex]->ActorPropChangeNotify();
	}
}

void UUnrealEdEngine::UpdateFloatingPropertyWindows(bool bForceRefresh)
{
	TArray<UObject*> SelectedObjects;

	// Assemble a set of valid selected actors.
	for (FSelectionIterator It = GetSelectedActorIterator(); It; ++It)
	{
		AActor* Actor = static_cast<AActor*>(*It);
		checkSlow(Actor->IsA(AActor::StaticClass()));

		if (!Actor->IsPendingKill())
		{
			SelectedObjects.Add(Actor);
		}
	}

	UpdateFloatingPropertyWindowsFromActorList(SelectedObjects, bForceRefresh);
}


void UUnrealEdEngine::UpdateFloatingPropertyWindowsFromActorList(const TArray<UObject*>& ActorList, bool bForceRefresh)
{
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	LevelEditor.BroadcastActorSelectionChanged(ActorList, bForceRefresh);
}
