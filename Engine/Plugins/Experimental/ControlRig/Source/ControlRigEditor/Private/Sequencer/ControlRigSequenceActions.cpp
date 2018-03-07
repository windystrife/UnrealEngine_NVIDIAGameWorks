// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequenceActions.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "ControlRigSequence.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FControlRigSequenceActions::FControlRigSequenceActions()
{
}

uint32 FControlRigSequenceActions::GetCategories()
{
	return EAssetTypeCategories::Animation;
}


FText FControlRigSequenceActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ControlRigSequence", "ControlRig Sequence");
}


UClass* FControlRigSequenceActions::GetSupportedClass() const
{
	return UControlRigSequence::StaticClass();
}


FColor FControlRigSequenceActions::GetTypeColor() const
{
	return FColor(108, 53, 0);
}


void FControlRigSequenceActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// forward to level sequence asset actions
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(ULevelSequence::StaticClass());
	if (AssetTypeActions.IsValid())
	{
		AssetTypeActions.Pin()->OpenAssetEditor(InObjects, EditWithinLevelEditor);
	}
}


bool FControlRigSequenceActions::ShouldForceWorldCentric()
{
	// @todo sequencer: Hack to force world-centric mode for Sequencer
	return true;
}

#undef LOCTEXT_NAMESPACE
