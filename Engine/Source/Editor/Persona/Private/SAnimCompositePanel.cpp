// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimCompositePanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Animation/EditorAnimCompositeSegment.h"

#include "SAnimSegmentsPanel.h"
#include "SAnimCompositeEditor.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE "AnimCompositePanel"

//////////////////////////////////////////////////////////////////////////
// SAnimCompositePanel

void SAnimCompositePanel::Construct(const FArguments& InArgs)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	Composite = InArgs._Composite;
	CompositeEditor = InArgs._CompositeEditor;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT( "CompositeLabel", "Composite" ) )
			.BodyContent()
			[
				SAssignNew( PanelArea, SBorder )
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				.Padding( FMargin(2.0f, 2.0f) )
				.ColorAndOpacity( FLinearColor::White )
			]
		]
	];

	Update();
}

void SAnimCompositePanel::Update()
{
	ClearSelected();
	if ( Composite != NULL )
	{
		SAnimCompositeEditor* Editor = CompositeEditor.Pin().Get();

		TSharedPtr<FTrackColorTracker> ColourTracker = MakeShareable(new FTrackColorTracker);
		ColourTracker->AddColor(FLinearColor(0.9f, 0.9f, 0.9f, 0.9f));
		ColourTracker->AddColor(FLinearColor(0.5f, 0.5f, 0.5f));

		FLinearColor NodeColor = FLinearColor(0.f, 0.5f, 0.0f, 0.5f);
		
		TSharedPtr<SVerticalBox> CompositeSlots;
		PanelArea->SetContent(
			SAssignNew( CompositeSlots, SVerticalBox )
			);

		TSharedRef<S2ColumnWidget> SectionTrack = Create2ColumnWidget(CompositeSlots.ToSharedRef());

		SectionTrack->LeftColumn->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			[
				SNew(SAnimSegmentsPanel)
				.AnimTrack(&Composite->AnimationTrack)
				.NodeSelectionSet(&SelectionSet)
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.ColorTracker(ColourTracker)
				.NodeColor(NodeColor)
				.ScrubPosition( Editor, &SAnimCompositeEditor::GetScrubValue )
				.TrackMaxValue(Composite->SequenceLength)
				.TrackNumDiscreteValues(Composite->GetNumberOfFrames())

				.OnAnimSegmentNodeClicked( this, &SAnimCompositePanel::ShowSegmentInDetailsView )
				.OnPreAnimUpdate( Editor, &SAnimCompositeEditor::PreAnimUpdate )
				.OnPostAnimUpdate( Editor, &SAnimCompositeEditor::PostAnimUpdate )
			];
	}
}

void SAnimCompositePanel::ShowSegmentInDetailsView(int32 SegmentIndex)
{
	UEditorAnimCompositeSegment *Obj = Cast<UEditorAnimCompositeSegment>(CompositeEditor.Pin()->ShowInDetailsView(UEditorAnimCompositeSegment::StaticClass()));
	if(Obj != NULL)
	{
		Obj->InitAnimSegment(SegmentIndex);
	}
}

void SAnimCompositePanel::ClearSelected()
{
	SelectionSet.Empty();
	CompositeEditor.Pin()->ClearDetailsView();
}

#undef LOCTEXT_NAMESPACE
