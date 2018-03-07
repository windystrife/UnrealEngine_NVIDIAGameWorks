// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerAddonBase.h"
#include "GameplayDebuggerCategoryReplicator.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

AActor* FGameplayDebuggerAddonBase::FindLocalDebugActor() const
{
	AGameplayDebuggerCategoryReplicator* RepOwnerOb = RepOwner.Get();
	return RepOwnerOb ? RepOwnerOb->GetDebugActor() : nullptr;
}

AGameplayDebuggerCategoryReplicator* FGameplayDebuggerAddonBase::GetReplicator() const
{
	return RepOwner.Get();
}

FString FGameplayDebuggerAddonBase::GetInputHandlerDescription(int32 HandlerId) const
{
	return InputHandlers.IsValidIndex(HandlerId) ? InputHandlers[HandlerId].ToString() : FString();
}

void FGameplayDebuggerAddonBase::OnGameplayDebuggerActivated()
{
	// empty in base class
}

void FGameplayDebuggerAddonBase::OnGameplayDebuggerDeactivated()
{
	// empty in base class
}

bool FGameplayDebuggerAddonBase::IsSimulateInEditor()
{
#if WITH_EDITOR
	extern UNREALED_API UEditorEngine* GEditor;
	return GIsEditor && (GEditor->bIsSimulateInEditorQueued || GEditor->bIsSimulatingInEditor);
#endif
	return false;
}
