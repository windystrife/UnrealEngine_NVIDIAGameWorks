// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TabFactory/SequencerTabSummoner.h"
#include "UMGStyle.h"

#define LOCTEXT_NAMESPACE "UMG"

const FName FSequencerTabSummoner::TabID(TEXT("Sequencer"));

FSequencerTabSummoner::FSequencerTabSummoner(TSharedPtr<class FWidgetBlueprintEditor> InBlueprintEditor)
		: FWorkflowTabFactory(TabID, InBlueprintEditor)
		, BlueprintEditor(InBlueprintEditor)
{
	TabLabel = LOCTEXT("SequencerLabel", "Timeline");
	TabIcon = FSlateIcon(FUMGStyle::GetStyleSetName(), "Sequencer.TabIcon");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("Sequencer_ViewMenu_Desc", "Timeline");
	ViewMenuTooltip = LOCTEXT("Sequencer_ViewMenu_ToolTip", "Show the Animation editor");
}

TSharedRef<SWidget> FSequencerTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FWidgetBlueprintEditor> BlueprintEditorPinned = BlueprintEditor.Pin();

	return BlueprintEditorPinned->CreateSequencerWidget();

}

#undef LOCTEXT_NAMESPACE 
