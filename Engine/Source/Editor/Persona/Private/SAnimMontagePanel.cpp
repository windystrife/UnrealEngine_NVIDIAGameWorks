// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimMontagePanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Animation/EditorAnimSegment.h"
#include "SAnimSegmentsPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Input/STextComboBox.h"
#include "SAnimTimingPanel.h"
#include "TabSpawners.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "AnimMontagePanel"

//////////////////////////////////////////////////////////////////////////
// SAnimMontagePanel

void SAnimMontagePanel::Construct(const FArguments& InArgs, FSimpleMulticastDelegate& OnAnimNotifiesChanged, FSimpleMulticastDelegate& OnSectionsChanged)
{
	SAnimTrackPanel::Construct( SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	Montage = InArgs._Montage;
	OnInvokeTab = InArgs._OnInvokeTab;
	MontageEditor = InArgs._MontageEditor;
	SectionTimingNodeVisibility = InArgs._SectionTimingNodeVisibility;
	OnSetMontagePreviewSlot = InArgs._OnSetMontagePreviewSlot;
	CurrentPreviewSlot = 0;

	bChildAnimMontage = InArgs._bChildAnimMontage;

	//TrackStyle = TRACK_Double;
	CurrentPosition = InArgs._CurrentPosition;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew( SExpandableArea )
			.AreaTitle( LOCTEXT("Montage", "Montage") )
			.BodyContent()
			[
				SAssignNew( PanelArea, SBorder )
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				.Padding( FMargin(2.0f, 2.0f) )
				.ColorAndOpacity( FLinearColor::White )
			]
		]
	];

	OnAnimNotifiesChanged.Add(FSimpleDelegate::CreateSP(this, &SAnimMontagePanel::Update));
	OnSectionsChanged.Add(FSimpleDelegate::CreateSP(this, &SAnimMontagePanel::Update));

	Update();
}

/** This is the main function that creates the UI widgets for the montage tool.*/
void SAnimMontagePanel::Update()
{
	if ( Montage != NULL )
	{
		CurrentPreviewSlot = FMath::Clamp(CurrentPreviewSlot, 0, Montage->SlotAnimTracks.Num() - 1);
		OnSetMontagePreviewSlot.ExecuteIfBound(CurrentPreviewSlot);

		SMontageEditor * Editor = MontageEditor.Pin().Get();
		int32 ColorIdx=0;
		//FLinearColor Colors[] = { FLinearColor(0.9f, 0.9f, 0.9f, 0.9f), FLinearColor(0.5f, 0.5f, 0.5f) };
		
		TSharedPtr<FTrackColorTracker> ColourTracker = MakeShareable(new FTrackColorTracker);
		ColourTracker->AddColor(FLinearColor(0.9f, 0.9f, 0.9f, 0.9f));
		ColourTracker->AddColor(FLinearColor(0.5f, 0.5f, 0.5f));

		FLinearColor NodeColor = FLinearColor(0.f, 0.5f, 0.0f, 0.5f);
		//FLinearColor SelectedColor = FLinearColor(1.0f,0.65,0.0f);

		TSharedPtr<SVerticalBox> MontageSlots;
		PanelArea->SetContent(
			SAssignNew( MontageSlots, SVerticalBox )
			);

		/************************************************************************/
		/* Status Bar                                                                     */
		/************************************************************************/
		{
			MontageSlots->AddSlot()
				.AutoHeight()
				[
					// Header, shows name of timeline we are editing
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush(TEXT("Graph.TitleBackground")))
					.HAlign(HAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.FillWidth(3.f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SAssignNew(StatusBarWarningImage, SImage)
							.Image(FEditorStyle::GetBrush("AnimSlotManager.Warning"))
							.Visibility(EVisibility::Hidden)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						[
							SAssignNew(StatusBarTextBlock, STextBlock)
							.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12))
							.ColorAndOpacity(FLinearColor(1, 1, 1, 0.5))
						]
					]
				];
		}

		// ===================================
		// Section Name Track
		// ===================================
		{
			TSharedRef<S2ColumnWidget> SectionTrack = Create2ColumnWidget(MontageSlots.ToSharedRef());

			SectionTrack->LeftColumn->ClearChildren();
			SectionTrack->LeftColumn->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding( FMargin(0.5f, 0.5f) )
				[
					SAssignNew(SectionNameTrack, STrack)
					.IsEnabled(!bChildAnimMontage)
					.ViewInputMin(ViewInputMin)
					.ViewInputMax(ViewInputMax)
					.TrackColor( ColourTracker->GetNextColor() )
					.OnBarDrag(Editor, &SMontageEditor::OnEditSectionTime)
					.OnBarDrop(Editor, &SMontageEditor::OnEditSectionTimeFinish)
					.OnBarClicked(SharedThis(this), &SAnimMontagePanel::ShowSectionInDetailsView)
					.DraggableBars(Editor, &SMontageEditor::GetSectionStartTimes)
					.DraggableBarSnapPositions(Editor, &SMontageEditor::GetAnimSegmentStartTimes)
					.DraggableBarLabels(Editor, &SMontageEditor::GetSectionNames)
					.TrackMaxValue(this, &SAnimMontagePanel::GetSequenceLength)
					.TrackNumDiscreteValues(Montage->GetNumberOfFrames())
					.OnTrackRightClickContextMenu( this, &SAnimMontagePanel::SummonTrackContextMenu, static_cast<int>(INDEX_NONE))
					.ScrubPosition( Editor, &SMontageEditor::GetScrubValue )
				];

			RefreshTimingNodes();
		}

		// ===================================
		// Anim Segment Tracks
		// ===================================
		{
			int32 NumAnimTracks = Montage->SlotAnimTracks.Num();

			SlotNameComboBoxes.Empty(NumAnimTracks);
			SlotNameComboSelectedNames.Empty(NumAnimTracks);
			SlotWarningImages.Empty(NumAnimTracks);
			SlotNameComboBoxes.AddZeroed(NumAnimTracks);
			SlotNameComboSelectedNames.AddZeroed(NumAnimTracks);
			SlotWarningImages.AddZeroed(NumAnimTracks);

			RefreshComboLists();
			check(SlotNameComboBoxes.Num() == NumAnimTracks);
			check(SlotNameComboSelectedNames.Num() == NumAnimTracks);

			for (int32 SlotAnimIdx = 0; SlotAnimIdx < NumAnimTracks; SlotAnimIdx++)
			{
				TSharedRef<S2ColumnWidget> SectionTrack = Create2ColumnWidget(MontageSlots.ToSharedRef());
				
				int32 FoundIndex = SlotNameList.Find(SlotNameComboSelectedNames[SlotAnimIdx]);
				TSharedPtr<FString> ComboItem = SlotNameComboListItems[FoundIndex];

				// Right column
				SectionTrack->RightColumn->AddSlot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.FillHeight(1)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SAssignNew(SlotNameComboBoxes[SlotAnimIdx], STextComboBox)
						.OptionsSource(&SlotNameComboListItems)
						.OnSelectionChanged(this, &SAnimMontagePanel::OnSlotNameChanged, SlotAnimIdx)
						.OnComboBoxOpening(this, &SAnimMontagePanel::OnSlotListOpening, SlotAnimIdx)
						.InitiallySelectedItem(ComboItem)
						.ContentPadding(2)
						.ToolTipText(FText::FromString(*ComboItem))
					]

					+ SVerticalBox::Slot()
						.HAlign(HAlign_Left)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							[
								SNew(SButton)
								.Text(LOCTEXT("AnimSlotNode_DetailPanelManageButtonLabel", "Anim Slot Manager"))
								.ToolTipText(LOCTEXT("AnimSlotNode_DetailPanelManageButtonToolTipText", "Open Anim Slot Manager to edit Slots and Groups."))
								.OnClicked(this, &SAnimMontagePanel::OnOpenAnimSlotManager)
								.Content()
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("MeshPaint.FindInCB"))
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SCheckBox)
								.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
								.ToolTipText(LOCTEXT("DetailPanelPreviewToolTipText", "Preview this slot in the editor"))
								.IsChecked(this, &SAnimMontagePanel::IsSlotPreviewed, SlotAnimIdx)
								.OnCheckStateChanged(this, &SAnimMontagePanel::OnSlotPreviewedChanged, SlotAnimIdx)
								[
									SNew(SBox)
									.VAlign(VAlign_Center)
									.HAlign(HAlign_Center)
									.Padding(FMargin(4.0, 2.0))
									[
										SNew(STextBlock)
										.Text(LOCTEXT("DetailPanelPreview", "Preview"))
									]
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.FillWidth(2.f)
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SAssignNew(SlotWarningImages[SlotAnimIdx], SImage)
								.Image(FEditorStyle::GetBrush("AnimSlotManager.Warning"))
								.Visibility(EVisibility::Hidden)
							]
						]
				];

				SectionTrack->RightColumn->SetEnabled(!bChildAnimMontage);
				if (bChildAnimMontage)
				{
					SectionTrack->LeftColumn->AddSlot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SAnimSegmentsPanel)
						.AnimTrack(&Montage->SlotAnimTracks[SlotAnimIdx].AnimTrack)
						.SlotName(Montage->SlotAnimTracks[SlotAnimIdx].SlotName)
						.NodeSelectionSet(&SelectionSet)
						.ViewInputMin(ViewInputMin)
						.ViewInputMax(ViewInputMax)
						.ColorTracker(ColourTracker)
						.bChildAnimMontage(bChildAnimMontage)
						.NodeColor(NodeColor)
						.OnPreAnimUpdate(Editor, &SMontageEditor::PreAnimUpdate)
						.OnPostAnimUpdate(Editor, &SMontageEditor::PostAnimUpdate)
						.OnAnimReplaceMapping(Editor, &SMontageEditor::ReplaceAnimationMapping)
						.OnDiffFromParentAsset(Editor, &SMontageEditor::IsDiffererentFromParent)
						.ScrubPosition(Editor, &SMontageEditor::GetScrubValue)
						.TrackMaxValue(this, &SAnimMontagePanel::GetSequenceLength)
						.TrackNumDiscreteValues(Montage->GetNumberOfFrames())
					];

				}
				else
				{
					SectionTrack->LeftColumn->AddSlot()
					.AutoHeight()
					.VAlign(VAlign_Center)
					[
						SNew(SAnimSegmentsPanel)
						.AnimTrack(&Montage->SlotAnimTracks[SlotAnimIdx].AnimTrack)
						.SlotName(Montage->SlotAnimTracks[SlotAnimIdx].SlotName)
						.NodeSelectionSet(&SelectionSet)
						.ViewInputMin(ViewInputMin)
						.ViewInputMax(ViewInputMax)
						.ColorTracker(ColourTracker)
						.bChildAnimMontage(bChildAnimMontage)
						.NodeColor(NodeColor)
						.DraggableBars(Editor, &SMontageEditor::GetSectionStartTimes)
						.DraggableBarSnapPositions(Editor, &SMontageEditor::GetAnimSegmentStartTimes)
						.ScrubPosition(Editor, &SMontageEditor::GetScrubValue)
						.TrackMaxValue(this, &SAnimMontagePanel::GetSequenceLength)
						.TrackNumDiscreteValues(Montage->GetNumberOfFrames())
						.OnAnimSegmentNodeClicked(this, &SAnimMontagePanel::ShowSegmentInDetailsView, SlotAnimIdx)
						.OnPreAnimUpdate(Editor, &SMontageEditor::PreAnimUpdate)
						.OnPostAnimUpdate(Editor, &SMontageEditor::PostAnimUpdate)
						.OnAnimSegmentRemoved(this, &SAnimMontagePanel::OnAnimSegmentRemoved, SlotAnimIdx)
						.OnBarDrag(Editor, &SMontageEditor::OnEditSectionTime)
						.OnBarDrop(Editor, &SMontageEditor::OnEditSectionTimeFinish)
						.OnBarClicked(SharedThis(this), &SAnimMontagePanel::ShowSectionInDetailsView)
						.OnTrackRightClickContextMenu(this, &SAnimMontagePanel::SummonTrackContextMenu, static_cast<int>(SlotAnimIdx))
					];
				}
			}
		}
	}

	UpdateSlotGroupWarningVisibility();
}

void SAnimMontagePanel::SetMontage(class UAnimMontage * InMontage)
{
	if (InMontage != Montage)
	{
		Montage = InMontage;
		Update();
	}
}

FReply SAnimMontagePanel::OnMouseButtonDown( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	FReply Reply = SAnimTrackPanel::OnMouseButtonDown( InMyGeometry, InMouseEvent );

	ClearSelected();

	return Reply;
}

void SAnimMontagePanel::SummonTrackContextMenu( FMenuBuilder& MenuBuilder, float DataPosX, int32 SectionIndex, int32 AnimSlotIndex )
{
	FUIAction UIAction;

	// Sections
	MenuBuilder.BeginSection("AnimMontageSections", LOCTEXT("Sections", "Sections"));
	{
		// New Action as we have a CanExecuteAction defined
		UIAction.ExecuteAction.BindRaw(this, &SAnimMontagePanel::OnNewSectionClicked, static_cast<float>(DataPosX));
		UIAction.CanExecuteAction.BindRaw(this, &SAnimMontagePanel::CanAddNewSection);
		MenuBuilder.AddMenuEntry(LOCTEXT("NewMontageSection", "New Montage Section"), LOCTEXT("NewMontageSectionToolTip", "Adds a new Montage Section"), FSlateIcon(), UIAction);
	
		UIAction.CanExecuteAction.Unbind();

		if (SectionIndex != INDEX_NONE && Montage->CompositeSections.Num() > 1) // Can't delete the last section!
		{
			UIAction.ExecuteAction.BindRaw(MontageEditor.Pin().Get(), &SMontageEditor::RemoveSection, SectionIndex);
			MenuBuilder.AddMenuEntry(LOCTEXT("DeleteMontageSection", "Delete Montage Section"), LOCTEXT("DeleteMontageSectionToolTip", "Deletes Montage Section"), FSlateIcon(), UIAction);

			FCompositeSection& Section = Montage->CompositeSections[SectionIndex];

			// Add item to directly set section time
			TSharedRef<SWidget> TimeWidget = 
				SNew( SBox )
				.HAlign( HAlign_Right )
				.ToolTipText(LOCTEXT("SetSectionTimeToolTip", "Set the time of this section directly"))
				[
					SNew(SBox)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<float>)
						.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.MinValue(0.0f)
						.MaxValue(Montage->SequenceLength)
						.Value(Section.GetTime())
						.AllowSpin(true)
						.OnValueCommitted_Lambda([this, SectionIndex](float InValue, ETextCommit::Type InCommitType)
						{
							if (Montage->CompositeSections.IsValidIndex(SectionIndex))
							{
								MontageEditor.Pin()->SetSectionTime(SectionIndex, InValue);
							}

							FSlateApplication::Get().DismissAllMenus();
						})
					]
				];

			MenuBuilder.AddWidget(TimeWidget, LOCTEXT("SectionTimeMenuText", "Section Time"));

			// Add item to directly set section frame
			TSharedRef<SWidget> FrameWidget = 
				SNew( SBox )
				.HAlign( HAlign_Right )
				.ToolTipText(LOCTEXT("SetFrameToolTip", "Set the frame of this section directly"))
				[
					SNew(SBox)
					.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.MinValue(0)
						.MaxValue(Montage->GetNumberOfFrames())
						.Value(Montage->GetFrameAtTime(Section.GetTime()))
						.AllowSpin(true)						
						.OnValueCommitted_Lambda([this, SectionIndex](int32 InValue, ETextCommit::Type InCommitType)
						{
							if (Montage->CompositeSections.IsValidIndex(SectionIndex))
							{
								float NewTime = FMath::Clamp(Montage->GetTimeAtFrame(InValue), 0.0f, Montage->SequenceLength);
								MontageEditor.Pin()->SetSectionTime(SectionIndex, NewTime);
							}

							FSlateApplication::Get().DismissAllMenus();
						})
					]
				];

			MenuBuilder.AddWidget(FrameWidget, LOCTEXT("SectionFrameMenuText", "Section Frame"));
		}
	}
	MenuBuilder.EndSection();

	// Slots
	MenuBuilder.BeginSection("AnimMontageSlots", LOCTEXT("Slots", "Slots") );
	{
		UIAction.ExecuteAction.BindRaw(this, &SAnimMontagePanel::OnNewSlotClicked);
		MenuBuilder.AddMenuEntry(LOCTEXT("NewSlot", "New Slot"), LOCTEXT("NewSlotToolTip", "Adds a new Slot"), FSlateIcon(), UIAction);

		if(AnimSlotIndex != INDEX_NONE)
		{
			UIAction.ExecuteAction.BindRaw(MontageEditor.Pin().Get(), &SMontageEditor::RemoveMontageSlot, AnimSlotIndex);
			UIAction.CanExecuteAction.BindRaw(MontageEditor.Pin().Get(), &SMontageEditor::CanRemoveMontageSlot, AnimSlotIndex);
			MenuBuilder.AddMenuEntry(LOCTEXT("DeleteSlot", "Delete Slot"), LOCTEXT("DeleteSlotToolTip", "Deletes Slot"), FSlateIcon(), UIAction);
			UIAction.CanExecuteAction.Unbind();

			UIAction.ExecuteAction.BindRaw(MontageEditor.Pin().Get(), &SMontageEditor::DuplicateMontageSlot, AnimSlotIndex);
			MenuBuilder.AddMenuEntry(LOCTEXT("DuplicateSlot", "Duplicate Slot"), LOCTEXT("DuplicateSlotToolTip", "Duplicates the slected slot"), FSlateIcon(), UIAction);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimMontageElementBulkActions", LOCTEXT("BulkLinkActions", "Bulk Link Actions"));
	{
		MenuBuilder.AddSubMenu(LOCTEXT("SetElementLink_SubMenu", "Set Elements to..."), LOCTEXT("SetElementLink_TooTip", "Sets all montage elements (Sections, Notifies) to a chosen link type."), FNewMenuDelegate::CreateSP(this, &SAnimMontagePanel::FillElementSubMenuForTimes));

		if(Montage->SlotAnimTracks.Num() > 1)
		{
			MenuBuilder.AddSubMenu(LOCTEXT("SetToSlotMenu", "Link all Elements to Slot..."), LOCTEXT("SetToSlotMenuToolTip", "Link all elements to a selected slot"), FNewMenuDelegate::CreateSP(this, &SAnimMontagePanel::FillSlotSubMenu));
		}
	}
	MenuBuilder.EndSection();

	LastContextHeading.Empty();
}

void SAnimMontagePanel::FillElementSubMenuForTimes(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(LOCTEXT("SubLinkAbs", "Absolute"), LOCTEXT("SubLinkAbsTooltip", "Set all elements to absolute link"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAnimMontagePanel::OnSetElementsToLinkMode, EAnimLinkMethod::Absolute)));
	MenuBuilder.AddMenuEntry(LOCTEXT("SubLinkRel", "Relative"), LOCTEXT("SubLinkRelTooltip", "Set all elements to relative link"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAnimMontagePanel::OnSetElementsToLinkMode, EAnimLinkMethod::Relative)));
	MenuBuilder.AddMenuEntry(LOCTEXT("SubLinkPro", "Proportional"), LOCTEXT("SubLinkProTooltip", "Set all elements to proportional link"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAnimMontagePanel::OnSetElementsToLinkMode, EAnimLinkMethod::Proportional)));
}

void SAnimMontagePanel::FillSlotSubMenu(FMenuBuilder& Menubuilder)
{
	for(int32 SlotIdx = 0 ; SlotIdx < Montage->SlotAnimTracks.Num() ; ++SlotIdx)
	{
		FSlotAnimationTrack& Slot = Montage->SlotAnimTracks[SlotIdx];
		Menubuilder.AddMenuEntry(FText::Format(LOCTEXT("SubSlotMenuNameEntry", "{SlotName}"), FText::FromString(Slot.SlotName.ToString())), LOCTEXT("SubSlotEntry", "Set to link to this slot"), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SAnimMontagePanel::OnSetElementsToSlot, SlotIdx)));
	}
}

/** Slots */
void SAnimMontagePanel::OnNewSlotClicked()
{
	MontageEditor.Pin()->AddNewMontageSlot(FAnimSlotGroup::DefaultSlotName);
}

void SAnimMontagePanel::CreateNewSlot(const FText& NewSlotName, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		MontageEditor.Pin()->AddNewMontageSlot(*NewSlotName.ToString());
	}

	FSlateApplication::Get().DismissAllMenus();
}

/** Sections */
void SAnimMontagePanel::OnNewSectionClicked(float DataPosX)
{
	// Show dialog to enter new track name
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label( LOCTEXT("NewSectionNameLabel", "Section Name") )
		.OnTextCommitted( this, &SAnimMontagePanel::CreateNewSection, DataPosX );


	// Show dialog to enter new event name
	FSlateApplication::Get().PushMenu(
		AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup )
		);
}

bool SAnimMontagePanel::CanAddNewSection()
{
	// Can't add sections if there isn't a montage, or that montage is of zero length
	return Montage && Montage->SequenceLength > 0.0f;
}

void SAnimMontagePanel::CreateNewSection(const FText& NewSectionName, ETextCommit::Type CommitInfo, float StartTime)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		MontageEditor.Pin()->AddNewSection(StartTime,NewSectionName.ToString());
	}
	FSlateApplication::Get().DismissAllMenus();
}

void SAnimMontagePanel::ShowSegmentInDetailsView(int32 AnimSegmentIndex, int32 AnimSlotIndex)
{
	UEditorAnimSegment *Obj = Cast<UEditorAnimSegment>(MontageEditor.Pin()->ShowInDetailsView(UEditorAnimSegment::StaticClass()));
	if(Obj != NULL)
	{
		Obj->InitAnimSegment(AnimSlotIndex,AnimSegmentIndex);
	}
}

void SAnimMontagePanel::ShowSectionInDetailsView(int32 SectionIndex)
{
	MontageEditor.Pin()->ShowSectionInDetailsView(SectionIndex);
}

void SAnimMontagePanel::ClearSelected()
{
	SelectionSet.Empty();
	MontageEditor.Pin()->ClearDetailsView();
}

void SAnimMontagePanel::OnSlotNameChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo, int32 AnimSlotIndex)
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		int32 ItemIndex = SlotNameComboListItems.Find(NewSelection);
		if (ItemIndex != INDEX_NONE)
		{
			FName NewSlotName = SlotNameList[ItemIndex];

			SlotNameComboSelectedNames[AnimSlotIndex] = NewSlotName;
			if (SlotNameComboBoxes[AnimSlotIndex].IsValid())
			{
				SlotNameComboBoxes[AnimSlotIndex]->SetToolTipText(FText::FromString(*NewSelection));
			}

			if (Montage->GetSkeleton()->ContainsSlotName(NewSlotName))
			{
				MontageEditor.Pin()->RenameSlotNode(AnimSlotIndex, NewSlotName.ToString());
			}
			
			UpdateSlotGroupWarningVisibility();

			// Clear selection, so Details panel for AnimNotifies doesn't show outdated information.
			ClearSelected();
		}
	}
}

ECheckBoxState SAnimMontagePanel::IsSlotPreviewed(int32 SlotIndex) const
{
	return (SlotIndex == CurrentPreviewSlot) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAnimMontagePanel::OnSlotPreviewedChanged(ECheckBoxState NewState, int32 SlotIndex)
{
	if (NewState == ECheckBoxState::Checked)
	{
		CurrentPreviewSlot = SlotIndex;
		OnSetMontagePreviewSlot.ExecuteIfBound(CurrentPreviewSlot);
	}
}

void SAnimMontagePanel::OnSlotListOpening(int32 AnimSlotIndex)
{
	// Refresh Slot Names, in case we used the Anim Slot Manager to make changes.
	RefreshComboLists(true);
}

FReply SAnimMontagePanel::OnOpenAnimSlotManager()
{
	OnInvokeTab.ExecuteIfBound(FPersonaTabs::SkeletonSlotNamesID);

	return FReply::Handled();
}

void SAnimMontagePanel::RefreshComboLists(bool bOnlyRefreshIfDifferent /*= false*/)
{
	// Make sure all slots defined in the montage are registered in our skeleton.
	int32 NumAnimTracks = Montage->SlotAnimTracks.Num();
	for (int32 TrackIndex = 0; TrackIndex < NumAnimTracks; TrackIndex++)
	{
		FName TrackSlotName = Montage->SlotAnimTracks[TrackIndex].SlotName;
		Montage->GetSkeleton()->RegisterSlotNode(TrackSlotName);
		SlotNameComboSelectedNames[TrackIndex] = TrackSlotName;
	}

	// Refresh Slot Names
	{
		TArray<TSharedPtr<FString>>	NewSlotNameComboListItems;
		TArray<FName> NewSlotNameList;

		bool bIsSlotNameListDifferent = false;

		const TArray<FAnimSlotGroup>& SlotGroups = Montage->GetSkeleton()->GetSlotGroups();
		for (auto SlotGroup : SlotGroups)
		{
			int32 Index = 0;
			for (auto SlotName : SlotGroup.SlotNames)
			{
				NewSlotNameList.Add(SlotName);

				FString ComboItemString = FString::Printf(TEXT("%s.%s"), *SlotGroup.GroupName.ToString(), *SlotName.ToString());
				NewSlotNameComboListItems.Add(MakeShareable(new FString(ComboItemString)));

				bIsSlotNameListDifferent = bIsSlotNameListDifferent || (!SlotNameComboListItems.IsValidIndex(Index) || (SlotNameComboListItems[Index] != NewSlotNameComboListItems[Index]));
				Index++;
			}
		}

		// Refresh if needed
		if (bIsSlotNameListDifferent || !bOnlyRefreshIfDifferent || (NewSlotNameComboListItems.Num() == 0))
		{
			SlotNameComboListItems = NewSlotNameComboListItems;
			SlotNameList = NewSlotNameList;

			// Update Combo Boxes
			for (int32 TrackIndex = 0; TrackIndex < NumAnimTracks; TrackIndex++)
			{
				if (SlotNameComboBoxes[TrackIndex].IsValid())
				{
					FName SelectedSlotName = SlotNameComboSelectedNames[TrackIndex];
					if (Montage->GetSkeleton()->ContainsSlotName(SelectedSlotName))
					{
						int32 FoundIndex = SlotNameList.Find(SelectedSlotName);
						TSharedPtr<FString> ComboItem = SlotNameComboListItems[FoundIndex];

						SlotNameComboBoxes[TrackIndex]->SetSelectedItem(ComboItem);
						SlotNameComboBoxes[TrackIndex]->SetToolTipText(FText::FromString(*ComboItem));
					}
					SlotNameComboBoxes[TrackIndex]->RefreshOptions();
				}
			}
		}
	}
}

void SAnimMontagePanel::UpdateSlotGroupWarningVisibility()
{
	bool bShowStatusBarWarning = false;
	FName MontageGroupName = Montage->GetGroupName();

	int32 NumAnimTracks = Montage->SlotAnimTracks.Num();
	if (NumAnimTracks > 0)
	{
		TArray<FName> UniqueSlotNameList;
		for (int32 TrackIndex = 0; TrackIndex < NumAnimTracks; TrackIndex++)
		{
			FName CurrentSlotName = SlotNameComboSelectedNames[TrackIndex];
			FName CurrentSlotGroupName = Montage->GetSkeleton()->GetSlotGroupName(CurrentSlotName);

			int32 SlotNameCount = 0;
			for (int32 Index = 0; Index < NumAnimTracks; Index++)
			{
				if (CurrentSlotName == SlotNameComboSelectedNames[Index])
				{
					SlotNameCount++;
				}
			}
			
			// Verify that slot names are unique.
			bool bSlotNameAlreadyInUse = UniqueSlotNameList.Contains(CurrentSlotName);
			if (!bSlotNameAlreadyInUse)
			{
				UniqueSlotNameList.Add(CurrentSlotName);
			}

			bool bDifferentGroupName = (CurrentSlotGroupName != MontageGroupName);
			bool bShowWarning = bDifferentGroupName || bSlotNameAlreadyInUse;
			bShowStatusBarWarning = bShowStatusBarWarning || bShowWarning;

			SlotWarningImages[TrackIndex]->SetVisibility(bShowWarning ? EVisibility::Visible : EVisibility::Hidden);
			if (bDifferentGroupName)
			{
				FText WarningText = FText::Format(LOCTEXT("AnimMontagePanel_SlotGroupMismatchToolTipText", "Slot's group '{0}' is different than the Montage's group '{1}'. All slots must belong to the same group."), FText::FromName(CurrentSlotGroupName), FText::FromName(MontageGroupName));
				SlotWarningImages[TrackIndex]->SetToolTipText(WarningText);
				StatusBarTextBlock->SetText(WarningText);
				StatusBarTextBlock->SetToolTipText(WarningText);
			}
			if (bSlotNameAlreadyInUse)
			{
				FText WarningText = FText::Format(LOCTEXT("AnimMontagePanel_SlotNameAlreadyInUseToolTipText", "Slot named '{0}' is already used in this Montage. All slots must be unique"), FText::FromName(CurrentSlotName));
				SlotWarningImages[TrackIndex]->SetToolTipText(WarningText);
				StatusBarTextBlock->SetText(WarningText);
				StatusBarTextBlock->SetToolTipText(WarningText);
			}
		}
	}

	// Update Status bar
	StatusBarWarningImage->SetVisibility(bShowStatusBarWarning ? EVisibility::Visible : EVisibility::Hidden);
	if (!bShowStatusBarWarning)
	{
		StatusBarTextBlock->SetText(FText::Format(LOCTEXT("AnimMontagePanel_StatusBarText", "Montage Group: '{0}'"), FText::FromName(MontageGroupName)));
		StatusBarTextBlock->SetToolTipText(LOCTEXT("AnimMontagePanel_StatusBarToolTipText", "The Montage Group is set by the first slot's group."));
	}
}

void SAnimMontagePanel::OnSetElementsToLinkMode(EAnimLinkMethod::Type NewLinkMethod)
{
	TArray<FAnimLinkableElement*> Elements;
	CollectLinkableElements(Elements);

	for(FAnimLinkableElement* Element : Elements)
	{
		Element->ChangeLinkMethod(NewLinkMethod);
	}

	// Handle notify state links
	for(FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		if(Notify.GetDuration() > 0.0f)
		{
			// Always keep link methods in sync between notifies and duration links
			if(Notify.GetLinkMethod() != Notify.EndLink.GetLinkMethod())
			{
				Notify.EndLink.ChangeLinkMethod(Notify.GetLinkMethod());
			}
		}
	}
}

void SAnimMontagePanel::OnSetElementsToSlot(int32 SlotIndex)
{
	TArray<FAnimLinkableElement*> Elements;
	CollectLinkableElements(Elements);

	for(FAnimLinkableElement* Element : Elements)
	{
		Element->ChangeSlotIndex(SlotIndex);
	}

	// Handle notify state links
	for(FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		if(Notify.GetDuration() > 0.0f)
		{
			// Always keep link methods in sync between notifies and duration links
			if(Notify.GetSlotIndex() != Notify.EndLink.GetSlotIndex())
			{
				Notify.EndLink.ChangeSlotIndex(Notify.GetSlotIndex());
			}
		}
	}
}

void SAnimMontagePanel::CollectLinkableElements(TArray<FAnimLinkableElement*> &Elements)
{
	for(auto& Composite : Montage->CompositeSections)
	{
		FAnimLinkableElement* Element = &Composite;
		Elements.Add(Element);
	}

	for(auto& Notify : Montage->Notifies)
	{
		FAnimLinkableElement* Element = &Notify;
		Elements.Add(Element);
	}
}

void SAnimMontagePanel::OnAnimSegmentRemoved(int32 SegmentIndex, int32 SlotIndex)
{
	TArray<FAnimLinkableElement*> LinkableElements;
	CollectLinkableElements(LinkableElements);

	for(FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		if(Notify.NotifyStateClass)
		{
			LinkableElements.Add(&Notify.EndLink);
		}
	}

	// Go through the linkable elements and fix the indices.
	// BG TODO: Once we can identify moved segments, remove
	for(FAnimLinkableElement* Element : LinkableElements)
	{
		if(Element->GetSlotIndex() == SlotIndex)
		{
			if(Element->GetSegmentIndex() == SegmentIndex)
			{
				Element->Clear();
			}
			else if(Element->GetSegmentIndex() > SegmentIndex)
			{
				Element->SetSegmentIndex(Element->GetSegmentIndex() - 1);
			}
		}
	}
}

float SAnimMontagePanel::GetSequenceLength() const
{
	if(Montage != nullptr)
	{
		return Montage->SequenceLength;
	}
	return 0.0f;
}

void SAnimMontagePanel::RefreshTimingNodes()
{
	if(SectionNameTrack.IsValid())
	{
		// Clear current nodes
		SectionNameTrack->ClearTrack();
		// Add section timing widgets
		TArray<TSharedPtr<FTimingRelevantElementBase>> TimingElements;
		TArray<TSharedRef<SAnimTimingNode>> SectionTimingNodes;
		SAnimTimingPanel::GetTimingRelevantElements(Montage, TimingElements);
		for(int32 Idx = 0 ; Idx < TimingElements.Num() ; ++Idx)
		{
			TSharedPtr<FTimingRelevantElementBase>& Element = TimingElements[Idx];
			if(Element->GetType() == ETimingElementType::Section)
			{
				TSharedRef<SAnimTimingTrackNode> Node = SNew(SAnimTimingTrackNode)
					.ViewInputMin(ViewInputMin)
					.ViewInputMax(ViewInputMax)
					.DataStartPos(Element->GetElementTime())
					.Element(Element)
					.bUseTooltip(false);

				Node->SetVisibility(SectionTimingNodeVisibility);

				SectionNameTrack->AddTrackNode
				(
					Node
				);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
