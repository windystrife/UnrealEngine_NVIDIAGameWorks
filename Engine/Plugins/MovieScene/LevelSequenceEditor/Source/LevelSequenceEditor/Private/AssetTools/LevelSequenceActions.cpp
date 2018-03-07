// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTools/LevelSequenceActions.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorToolkit.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FLevelSequenceActions constructors
 *****************************************************************************/

FLevelSequenceActions::FLevelSequenceActions( const TSharedRef<ISlateStyle>& InStyle )
	: Style(InStyle)
{ }


/* IAssetTypeActions interface
 *****************************************************************************/

uint32 FLevelSequenceActions::GetCategories()
{
	return EAssetTypeCategories::Animation;
}


FText FLevelSequenceActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LevelSequence", "Level Sequence");
}


UClass* FLevelSequenceActions::GetSupportedClass() const
{
	return ULevelSequence::StaticClass(); 
}


FColor FLevelSequenceActions::GetTypeColor() const
{
	return FColor(200, 80, 80);
}


void FLevelSequenceActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	UWorld* WorldContext = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			WorldContext = Context.World();
			break;
		}
	}

	if (!ensure(WorldContext))
	{
		return;
	}

	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		ULevelSequence* LevelSequence = Cast<ULevelSequence>(*ObjIt);

		if (LevelSequence != nullptr)
		{
			// Legacy upgrade
			LevelSequence->ConvertPersistentBindingsToDefault(WorldContext);

			TSharedRef<FLevelSequenceEditorToolkit> Toolkit = MakeShareable(new FLevelSequenceEditorToolkit(Style));
			Toolkit->Initialize(Mode, EditWithinLevelEditor, LevelSequence);
		}
	}
}


bool FLevelSequenceActions::ShouldForceWorldCentric()
{
	// @todo sequencer: Hack to force world-centric mode for Sequencer
	return true;
}


#undef LOCTEXT_NAMESPACE
