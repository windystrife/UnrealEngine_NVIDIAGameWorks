// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SAnimMontageSectionsPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "SMontageEditor.h"
#include "Widgets/Input/SButton.h"

#include "ScopedTransaction.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE "AnimMontageSectionsPanel"



//////////////////////////////////////////////////////////////////////////
// SAnimMontagePanel

void SAnimMontageSectionsPanel::Construct(const FArguments& InArgs)
{
	MontageEditor = InArgs._MontageEditor;
	Montage = InArgs._Montage;
	SelectedCompositeSection = INDEX_NONE;
	bChildAnimMontage = InArgs._bChildAnimMontage;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Sections", "Sections") )
			.BodyContent()
			[
				SNew( SBorder )
				.Padding( FMargin(2.f, 2.f) )
				[
					SAssignNew( PanelArea, SBorder )
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.Padding( FMargin(2.0f, 2.0f) )
					.ColorAndOpacity( FLinearColor::White )
				]
			]
		]
	];

	Update();
}

void SAnimMontageSectionsPanel::Update()
{
	int32 ColorIdx=0;
	FLinearColor Colors[] = { FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), FLinearColor(0.5f, 0.5f, 0.5f) };
	FLinearColor NodeColor = FLinearColor(0.f, 0.5f, 0.0f, 0.5f);
	FLinearColor SelectedColor = FLinearColor(1.0f,0.65,0.0f);
	FLinearColor LoopColor = FLinearColor(0.0f, 0.25f, 0.25f, 0.5f);

	
	if ( Montage != NULL )
	{
		TSharedPtr<STrack> Track;
		TSharedPtr<SVerticalBox> MontageSlots;
		PanelArea->SetContent(
			SAssignNew( MontageSlots, SVerticalBox )
			);

		SectionMap.Empty();
		TopSelectionSet.Empty();
		SelectionSet.Empty();
		MontageSlots->ClearChildren();

		SMontageEditor * Editor = MontageEditor.Pin().Get();
		
		TArray<bool>	Used;
		Used.AddZeroed(Montage->CompositeSections.Num());

		int RowIdx=0;

		/** Create Buttons for reseting/creating default section ordering */
		MontageSlots->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( FMargin(0.5f, 0.5f) )
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[				
					SNew(SButton)
					.IsEnabled(!bChildAnimMontage)
					.Visibility( EVisibility::Visible )
					.Text( LOCTEXT("CreateDefault", "Create Default") )
					.ToolTipText( LOCTEXT("CreateDefaultToolTip", "Reconstructs section ordering based on start time") )
					.OnClicked(this, &SAnimMontageSectionsPanel::MakeDefaultSequence)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[				
					SNew(SButton)
					.IsEnabled(!bChildAnimMontage)
					.Visibility( EVisibility::Visible )
					.Text( LOCTEXT("Clear", "Clear") )
					.ToolTipText( LOCTEXT("ClearToolTip", "Resets section orderings") )
					.OnClicked(this, &SAnimMontageSectionsPanel::ClearSequence)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
				]
			];


		/** Create top track of section nodes */
		MontageSlots->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( FMargin(0.5f, 20.0f) )
			[
				SAssignNew(Track, STrack)
				.IsEnabled(!bChildAnimMontage)
				.ViewInputMin(0)
				.ViewInputMax(100)
				.TrackColor( FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
				.TrackMaxValue(100)
				
			];

		for(int32 SectionIdx=0; SectionIdx < Montage->CompositeSections.Num(); SectionIdx++)
		{
			const float NodeLength = 100.f / static_cast<float>(Montage->CompositeSections.Num()+1);
			const float NodeSpacing = 100.f / static_cast<float>(Montage->CompositeSections.Num());

			Track->AddTrackNode(
				SNew(STrackNode)
				.ViewInputMax(100)
				.ViewInputMin(0)
				.NodeColor(NodeColor)
				.SelectedNodeColor(SelectedColor)
				.DataLength(NodeLength)
				.DataStartPos(NodeSpacing * SectionIdx)
				.NodeName(Montage->CompositeSections[SectionIdx].SectionName.ToString())
				.NodeSelectionSet(&TopSelectionSet)
				.OnTrackNodeClicked( this, &SAnimMontageSectionsPanel::TopSectionClicked, SectionIdx)
				.AllowDrag(false)
				);
		}

		MontageSlots->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding( FMargin(0.5f, 0.0f) )
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[				
					SNew(SButton)
					.Visibility( EVisibility::Visible )
					.Text( LOCTEXT("PreviewAll", "Preview All Sections") )
					.ToolTipText( LOCTEXT("PreviewAllToolTip", "Preview all sections in order they are") )
					.OnClicked(this, &SAnimMontageSectionsPanel::PreviewAllSectionsClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
				]
			];

		/** Create as many tracks as necessary to show each section at least once
		  * -Each track represents one chain of sections (section->next->next)
		  */

		while(true)
		{
			int32 SectionIdx = 0;
			TArray<bool>	UsedInThisRow;
			UsedInThisRow.AddZeroed(Montage->CompositeSections.Num());
			
			/** Find first section we haven't shown yet */
			for(;SectionIdx < Montage->CompositeSections.Num(); SectionIdx++)
			{
				if(!Used[SectionIdx])
					break;
			}

			if(SectionIdx >= Montage->CompositeSections.Num())
			{
				// Ran out of stuff to show - done
				break;
			}

			/** Create new track */
			SectionMap.Add( TArray<int32>() );
			MontageSlots->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding( FMargin(0.5f, 0.5f) )
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[				
						SNew(SButton)
						.Visibility( EVisibility::Visible )
						.Text( LOCTEXT("Preview", "Preview") )
						.ToolTipText( LOCTEXT("PreviewToolTip", "Preview this track") )
						.OnClicked(this, &SAnimMontageSectionsPanel::PreviewSectionClicked, SectionIdx)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(Track, STrack)
						.ViewInputMin(0)
						.ViewInputMax(100)
						.TrackColor( Colors[ColorIdx++ & 1])
						.TrackMaxValue(100)
					]
				];
			
			/** Add each track in this chain to the track we just created */
			int count =0;
			float TrackPos = 0;
			const float BarLength = 8;
			const float XLength = 1;
			while(true)
			{
				/** Add section if it hasn't already been used in this row (if its used in another row, thats ok) */
				if(Montage->IsValidSectionIndex(SectionIdx) && UsedInThisRow[SectionIdx]==false)
				{
					UsedInThisRow[SectionIdx] = true;
					Used[SectionIdx] = true;

					SectionMap[RowIdx].Add(SectionIdx);
				
					Track->AddTrackNode(
						SNew(STrackNode)
						.IsEnabled(!bChildAnimMontage)
						.ViewInputMax(100)
						.ViewInputMin(0)
						.NodeColor( IsLoop(SectionIdx) ? LoopColor : NodeColor)
						.SelectedNodeColor(SelectedColor)
						.DataLength(BarLength)
						.DataStartPos(TrackPos)
						.NodeName(Montage->CompositeSections[SectionIdx].SectionName.ToString())
						.OnTrackNodeDragged( this, &SAnimMontageSectionsPanel::SetSectionPos, SectionIdx, RowIdx)
						.OnTrackNodeDropped( this, &SAnimMontageSectionsPanel::OnSectionDrop)
						.OnTrackNodeClicked( this, &SAnimMontageSectionsPanel::SectionClicked, SectionIdx)
						.NodeSelectionSet(&SelectionSet)
						.AllowDrag(false)
					);
					TrackPos += BarLength + 0.25f;

					/** If this has a next section, create an X to delete that link */
					if(Montage->CompositeSections[SectionIdx].NextSectionName != NAME_None)
					{
						Track->AddTrackNode(
							SNew(STrackNode)
							.IsEnabled(!bChildAnimMontage)
							.ViewInputMax(100)
							.ViewInputMin(0)
							.NodeColor(IsLoop(SectionIdx) ? LoopColor : NodeColor)
							.SelectedNodeColor(SelectedColor)
							.DataStartPos(TrackPos)
							.NodeName(TEXT("x"))
							.OnTrackNodeDropped( this, &SAnimMontageSectionsPanel::OnSectionDrop)
							.OnTrackNodeClicked( this, &SAnimMontageSectionsPanel::RemoveLink, SectionIdx)
							.NodeSelectionSet(&SelectionSet)
							.AllowDrag(false)
							);

						TrackPos += XLength + 0.25;
					}

					count++;
					SectionIdx = Montage->GetSectionIndex( Montage->CompositeSections[SectionIdx].NextSectionName );

					continue;
				} else
				{
					break;
				}
			}

		}

		RowIdx++;

	}
	SelectedCompositeSection = INDEX_NONE;
}

void SAnimMontageSectionsPanel::SetSectionPos(float NewPos, int32 SectionIdx, int32 RowIdx)
{
	// Drag drop of sections not implemented yet
}

void SAnimMontageSectionsPanel::OnSectionDrop()
{
	Update();
}

void SAnimMontageSectionsPanel::TopSectionClicked(int32 SectionIndex)
{
	// Top section was clicked, add it to next section if possible
	if(Montage && Montage->IsValidSectionIndex(SelectedCompositeSection))
	{
		const FScopedTransaction Transaction( LOCTEXT("OnAddSectionToSectionChain", "Add Section to Composite Sections") );
		Montage->Modify();

		FName PrevNext = Montage->CompositeSections[SelectedCompositeSection].NextSectionName;

		Montage->CompositeSections[SelectedCompositeSection].NextSectionName = Montage->CompositeSections[SectionIndex].SectionName;
		MontageEditor.Pin()->RestartPreview();
		Update();

		Montage->PostEditChange();
	}
	MontageEditor.Pin()->ShowSectionInDetailsView(SectionIndex);
	TopSelectionSet.Empty();
}

FReply SAnimMontageSectionsPanel::PreviewAllSectionsClicked()
{
	MontageEditor.Pin()->RestartPreviewPlayAllSections();
	return FReply::Handled();
}

FReply SAnimMontageSectionsPanel::PreviewSectionClicked(int32 SectionIndex)
{
	MontageEditor.Pin()->RestartPreviewFromSection(SectionIndex);
	return FReply::Handled();
}

void SAnimMontageSectionsPanel::SectionClicked(int32 SectionIndex)
{
	// Other section was clicked
	SelectedCompositeSection = SectionIndex;
	MontageEditor.Pin()->ShowSectionInDetailsView(SectionIndex);
}

void SAnimMontageSectionsPanel::RemoveLink(int32 SectionIndex)
{
	if(Montage && Montage->IsValidSectionIndex(SectionIndex))
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveNextSection", "Remove Next Section from Composite") );
		Montage->Modify();
		
		Montage->CompositeSections[SectionIndex].NextSectionName = NAME_None;
		MontageEditor.Pin()->RestartPreview();
		Update();

		Montage->PostEditChange();
	}
}

FReply SAnimMontageSectionsPanel::MakeDefaultSequence()
{
	if(MontageEditor.Pin().IsValid())
	{
		MontageEditor.Pin().Get()->MakeDefaultSequentialSections();
		Update();
	}
	return FReply::Handled();
}

FReply SAnimMontageSectionsPanel::ClearSequence()
{
	if(MontageEditor.Pin().IsValid())
	{
		MontageEditor.Pin().Get()->ClearSquenceOrdering();
		Update();
	}
	return FReply::Handled();
}

bool SAnimMontageSectionsPanel::IsLoop(int32 SectionIdx)
{
	if(Montage && Montage->CompositeSections.IsValidIndex(SectionIdx))
	{
		TArray<bool>	Used;
		Used.AddZeroed(Montage->CompositeSections.Num());
		int32 CurrentIdx = SectionIdx;
		while(true)
		{
			CurrentIdx = Montage->GetSectionIndex(Montage->CompositeSections[CurrentIdx].NextSectionName);
			if(CurrentIdx == INDEX_NONE)
			{
				// End of the chain
				return false;
			}
			if(CurrentIdx == SectionIdx)
			{
				// Hit the starting position, return true
				return true;
			}
			if(Used[CurrentIdx])
			{
				// Hit a loop but the starting section was part of it, so return false
				return false;
			}
			Used[CurrentIdx]=true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
