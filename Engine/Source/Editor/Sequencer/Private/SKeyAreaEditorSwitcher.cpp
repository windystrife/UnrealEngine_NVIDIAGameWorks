// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SKeyAreaEditorSwitcher.h"
#include "SOverlay.h"
#include "SBox.h"
#include "SBoxPanel.h"
#include "Sequencer.h"
#include "SKeyNavigationButtons.h"
#include "IKeyArea.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"

void SKeyAreaEditorSwitcher::Construct(const FArguments& InArgs, TSharedRef<FSequencerSectionKeyAreaNode> InKeyAreaNode)
{
	WeakKeyAreaNode = InKeyAreaNode;
	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SAssignNew(Overlay, SOverlay)
				.IsEnabled(!InKeyAreaNode->GetSequencer().IsReadOnly())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SKeyNavigationButtons, InKeyAreaNode)
			]
		]
	];

	Rebuild();
}

void SKeyAreaEditorSwitcher::Rebuild()
{
	Overlay->ClearChildren();
	VisibleIndex = -1;

	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = WeakKeyAreaNode.Pin();
	if (!KeyAreaNode.IsValid())
	{
		return;
	}

	for (int32 Index = 0; Index < KeyAreaNode->GetAllKeyAreas().Num(); ++Index)
	{
		TSharedRef<IKeyArea> KeyArea = KeyAreaNode->GetAllKeyAreas()[Index];

		if (KeyArea->CanCreateKeyEditor())
		{
			Overlay->AddSlot()
			[
				SNew(SBox)
				.IsEnabled(!KeyAreaNode->GetSequencer().IsReadOnly())
				.WidthOverride(100)
				.HAlign(HAlign_Left)
				.Visibility(this, &SKeyAreaEditorSwitcher::GetWidgetVisibility, Index)
				[
					KeyArea->CreateKeyEditor(&KeyAreaNode->GetSequencer())
				]
			];
		}
	}
}

EVisibility SKeyAreaEditorSwitcher::GetWidgetVisibility(int32 Index) const
{
	return Index == VisibleIndex ? EVisibility::Visible : EVisibility::Collapsed;
}

void SKeyAreaEditorSwitcher::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	VisibleIndex = -1;

	TSharedPtr<FSequencerSectionKeyAreaNode> KeyAreaNode = WeakKeyAreaNode.Pin();
	if (!KeyAreaNode.IsValid() || KeyAreaNode->GetAllKeyAreas().Num() == 0)
	{
		return;
	}

	TArray<UMovieSceneSection*> AllSections;
	for (const TSharedRef<IKeyArea>& KeyArea : KeyAreaNode->GetAllKeyAreas())
	{
		AllSections.Add(KeyArea->GetOwningSection());
	}

	VisibleIndex = SequencerHelpers::GetSectionFromTime(AllSections, KeyAreaNode->GetSequencer().GetLocalTime());
}
