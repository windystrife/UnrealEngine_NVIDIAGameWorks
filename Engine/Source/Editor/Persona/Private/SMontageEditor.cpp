// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SMontageEditor.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/EditorCompositeSection.h"
#include "IDocumentation.h"

#include "SAnimTimingPanel.h"
#include "SAnimNotifyPanel.h"
#include "SAnimMontageScrubPanel.h"
#include "SAnimMontagePanel.h"
#include "SAnimMontageSectionsPanel.h"
#include "ScopedTransaction.h"
#include "AnimPreviewInstance.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Factories/AnimMontageFactory.h"

#define LOCTEXT_NAMESPACE "AnimSequenceEditor"

//////////////////////////////////////////////////////////////////////////
// SMontageEditor

TSharedRef<SWidget> SMontageEditor::CreateDocumentAnchor()
{
	return IDocumentation::Get()->CreateAnchor(TEXT("Engine/Animation/AnimMontage"));
}

void SMontageEditor::Construct(const FArguments& InArgs, const FMontageEditorRequiredArgs& InRequiredArgs)
{
	MontageObj = InArgs._Montage;
	check(MontageObj);
	OnSectionsChanged = InArgs._OnSectionsChanged;
	MontageObj->RegisterOnMontageChanged(UAnimMontage::FOnMontageChanged::CreateSP(this, &SMontageEditor::RebuildMontagePanel, false));
		
	EnsureStartingSection();
	EnsureSlotNode();

	// set child montage if montage has parent
	bChildAnimMontage = MontageObj->HasParentAsset();

	bDragging = false;
	bIsActiveTimerRegistered = false;

	SAnimEditorBase::Construct( SAnimEditorBase::FArguments()
		.OnObjectsSelected(InArgs._OnObjectsSelected), 
		InRequiredArgs.PreviewScene );

	InRequiredArgs.OnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SMontageEditor::PostUndo));

	SAssignNew(AnimTimingPanel, SAnimTimingPanel, InRequiredArgs.OnAnimNotifiesChanged, InRequiredArgs.OnSectionsChanged)
		.IsEnabled(!bChildAnimMontage)
		.InSequence(MontageObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange);

	TAttribute<EVisibility> SectionVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible, ETimingElementType::Section));
	TAttribute<EVisibility> NotifyVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible, ETimingElementType::QueuedNotify));
	FOnGetTimingNodeVisibility TimingNodeVisibilityDelegate = FOnGetTimingNodeVisibility::CreateSP(AnimTimingPanel.ToSharedRef(), &SAnimTimingPanel::IsElementDisplayVisible);
	
	if (bChildAnimMontage)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ParentClassName"), FText::FromString(GetNameSafe(MontageObj->ParentAsset)) );

		// add child montage warning section - and link to parent
		EditorPanels->AddSlot()
		.AutoHeight()
		.Padding(0, 0)
		[
			SNew(SBorder)
			.Padding(FMargin(10.f, 4.f))
			.BorderImage(FEditorStyle::GetBrush(TEXT("Graph.InstructionBackground")))
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
			.HAlign(HAlign_Center)
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.8f))
			[
				SNew(SVerticalBox)
			
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ParentAnimMontageLink", " This is a child anim montage. To edit the lay out, please go to the parent montage "))
						.TextStyle(FEditorStyle::Get(), "Persona.MontageEditor.ChildMontageInstruction")
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SMontageEditor::OnFindParentClassInContentBrowserClicked)
						.ToolTipText(LOCTEXT("FindParentInCBToolTip", "Find parent in Content Browser"))
						.ForegroundColor(FSlateColor::UseForeground())
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Browse"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.VAlign(VAlign_Center)
						.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SMontageEditor::OnEditParentClassClicked)
						.ToolTipText(LOCTEXT("EditParentClassToolTip", "Open parent in editor"))
						.ForegroundColor(FSlateColor::UseForeground())
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Edit"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RemapHelpText", " To remap an asset to a different asset, use the context menu or drag and drop the animation on to the segment."))
					.TextStyle(FEditorStyle::Get(), "Persona.MontageEditor.ChildMontageInstruction")
				]
			]
		];
	}

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 5)
	[
		SAssignNew( AnimMontagePanel, SAnimMontagePanel, InRequiredArgs.OnAnimNotifiesChanged, InRequiredArgs.OnSectionsChanged)
		.Montage(MontageObj)
		.bChildAnimMontage(bChildAnimMontage)
		.MontageEditor(SharedThis(this))
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.SectionTimingNodeVisibility(SectionVisibility)
		.OnInvokeTab(InArgs._OnInvokeTab)
		.OnSetMontagePreviewSlot(this, &SMontageEditor::OnSetMontagePreviewSlot)
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimMontageSectionsPanel, SAnimMontageSectionsPanel )
		.bChildAnimMontage(bChildAnimMontage)
		.Montage(MontageObj)
		.MontageEditor(SharedThis(this))
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		AnimTimingPanel.ToSharedRef()
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimNotifyPanel, SAnimNotifyPanel, InRequiredArgs.OnPostUndo )
		.Sequence(MontageObj)
		.IsEnabled(!bChildAnimMontage)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
		.OnSelectionChanged(this, &SAnimEditorBase::OnSelectionChanged)
		.MarkerBars(this, &SMontageEditor::GetMarkerBarInformation)
		.OnRequestRefreshOffsets(this, &SMontageEditor::RefreshNotifyTriggerOffsets)
		.OnGetTimingNodeVisibility(TimingNodeVisibilityDelegate)
		.OnAnimNotifiesChanged(InArgs._OnAnimNotifiesChanged)
		.OnInvokeTab(InArgs._OnInvokeTab)
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimCurvePanel, SAnimCurvePanel, InRequiredArgs.EditableSkeleton )
		.IsEnabled(!bChildAnimMontage)
		.Sequence(MontageObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
	];

	CollapseMontage();
}

FReply SMontageEditor::OnFindParentClassInContentBrowserClicked()
{
	if (MontageObj != NULL)
	{
		UObject* ParentClass = MontageObj->ParentAsset;
		if (ParentClass != NULL)
		{
			TArray< UObject* > ParentObjectList;
			ParentObjectList.Add(ParentClass);
			GEditor->SyncBrowserToObjects(ParentObjectList);
		}
	}

	return FReply::Handled();
}

FReply SMontageEditor::OnEditParentClassClicked()
{
	if (MontageObj != NULL)
	{
		UObject* ParentClass = MontageObj->ParentAsset;
		if (ParentClass != NULL)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ParentClass);
		}
	}

	return FReply::Handled();
}

SMontageEditor::~SMontageEditor()
{
	if (MontageObj)
	{
		MontageObj->UnregisterOnMontageChanged(this);
	}
}

TSharedRef<class SAnimationScrubPanel> SMontageEditor::ConstructAnimScrubPanel()
{
	return SAssignNew(AnimMontageScrubPanel, SAnimMontageScrubPanel, GetPreviewScene())
		.LockedSequence(MontageObj)
		.ViewInputMin(this, &SMontageEditor::GetViewMinInput)
		.ViewInputMax(this, &SMontageEditor::GetViewMaxInput)
		.OnSetInputViewRange(this, &SMontageEditor::SetInputViewRange)
		.bAllowZoom(true)
		.MontageEditor(SharedThis(this));
}

void SMontageEditor::SetMontageObj(UAnimMontage * NewMontage)
{
	MontageObj = NewMontage;

	if (MontageObj)
	{
		SetInputViewRange(0, MontageObj->SequenceLength); // FIXME
	}

	AnimMontagePanel->SetMontage(NewMontage);
	AnimNotifyPanel->SetSequence(NewMontage);
	AnimCurvePanel->SetSequence(NewMontage);
	// sequence editor locks the sequence, so it doesn't get replaced by clicking 
	AnimMontageScrubPanel->ReplaceLockedSequence(NewMontage);
}

void SMontageEditor::OnSetMontagePreviewSlot(int32 SlotIndex)
{
	UAnimSingleNodeInstance * PreviewInstance = GetPreviewInstance();
	if (PreviewInstance && MontageObj->SlotAnimTracks.IsValidIndex(SlotIndex))
	{
		const FName SlotName = MontageObj->SlotAnimTracks[SlotIndex].SlotName;
		PreviewInstance->SetMontagePreviewSlot(SlotName);
	}
}

bool SMontageEditor::ValidIndexes(int32 AnimSlotIndex, int32 AnimSegmentIndex) const
{
	return (MontageObj != NULL && MontageObj->SlotAnimTracks.IsValidIndex(AnimSlotIndex) && MontageObj->SlotAnimTracks[AnimSlotIndex].AnimTrack.AnimSegments.IsValidIndex(AnimSegmentIndex) );
}

bool SMontageEditor::ValidSection(int32 SectionIndex) const
{
	return (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex));
}

void SMontageEditor::RefreshNotifyTriggerOffsets()
{
	for(auto Iter = MontageObj->Notifies.CreateIterator(); Iter; ++Iter)
	{
		FAnimNotifyEvent& Notify = (*Iter);

		// Offset for the beginning of a notify
		EAnimEventTriggerOffsets::Type PredictedOffset = MontageObj->CalculateOffsetForNotify(Notify.GetTime());
		Notify.RefreshTriggerOffset(PredictedOffset);

		// Offset for the end of a notify state if necessary
		if(Notify.GetDuration() > 0.0f)
		{
			PredictedOffset = MontageObj->CalculateOffsetForNotify(Notify.GetTime() + Notify.GetDuration());
			Notify.RefreshEndTriggerOffset(PredictedOffset);
		}
		else
		{
			Notify.EndTriggerTimeOffset = 0.0f;
		}
	}
}

bool SMontageEditor::GetSectionTime( int32 SectionIndex, float &OutTime ) const
{
	if (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex))
	{
		OutTime = MontageObj->CompositeSections[SectionIndex].GetTime();
		return true;
	}

	return false;
}

TArray<FString>	SMontageEditor::GetSectionNames() const
{
	TArray<FString> Names;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			Names.Add(MontageObj->CompositeSections[I].SectionName.ToString());
		}
	}
	return Names;
}

TArray<float> SMontageEditor::GetSectionStartTimes() const
{
	TArray<float>	Times;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			Times.Add(MontageObj->CompositeSections[I].GetTime());
		}
	}
	return Times;
}

TArray<FTrackMarkerBar> SMontageEditor::GetMarkerBarInformation() const
{
	TArray<FTrackMarkerBar> MarkerBars;
	if (MontageObj != NULL)
	{
		for( int32 I=0; I < MontageObj->CompositeSections.Num(); I++)
		{
			FTrackMarkerBar Bar;
			Bar.Time = MontageObj->CompositeSections[I].GetTime();
			Bar.DrawColour = FLinearColor(0.f,1.f,0.f);
			MarkerBars.Add(Bar);
		}
	}
	return MarkerBars;
}

TArray<float> SMontageEditor::GetAnimSegmentStartTimes() const
{
	TArray<float>	Times;
	if (MontageObj != NULL)
	{
		for ( int32 i=0; i < MontageObj->SlotAnimTracks.Num(); i++)
		{
			for (int32 j=0; j < MontageObj->SlotAnimTracks[i].AnimTrack.AnimSegments.Num(); j++)
			{
				Times.Add( MontageObj->SlotAnimTracks[i].AnimTrack.AnimSegments[j].StartPos );
			}
		}
	}
	return Times;
}

void SMontageEditor::OnEditSectionTime( int32 SectionIndex, float NewTime)
{
	if (MontageObj != NULL && MontageObj->CompositeSections.IsValidIndex(SectionIndex))
	{
		if(!bDragging)
		{
			//If this is the first drag event
			const FScopedTransaction Transaction( LOCTEXT("EditSection", "Edit Section Start Time") );
			MontageObj->Modify();
		}
		bDragging = true;

		MontageObj->CompositeSections[SectionIndex].SetTime(NewTime);
		MontageObj->CompositeSections[SectionIndex].LinkMontage(MontageObj, NewTime);
	}

	AnimMontagePanel->RefreshTimingNodes();
}
void SMontageEditor::OnEditSectionTimeFinish( int32 SectionIndex )
{
	bDragging = false;

	if(MontageObj!=NULL)
	{
		SortSections();
		RefreshNotifyTriggerOffsets();
		OnMontageModified();
		AnimMontageSectionsPanel->Update();
	}

	OnSectionsChanged.ExecuteIfBound();
}

void SMontageEditor::SetSectionTime(int32 SectionIndex, float NewTime)
{
	if(MontageObj && MontageObj->CompositeSections.IsValidIndex(SectionIndex))
	{
		const FScopedTransaction Transaction(LOCTEXT("EditSection", "Edit Section Start Time"));
		MontageObj->Modify();
	
		FCompositeSection& Section = MontageObj->CompositeSections[SectionIndex];
		Section.SetTime(NewTime);
		Section.LinkMontage(MontageObj, NewTime);

		OnEditSectionTimeFinish(SectionIndex);
	}
}

void SMontageEditor::PreAnimUpdate()
{
	MontageObj->Modify();
}

void SMontageEditor::OnMontageModified()
{
	MontageObj->PostEditChange();
	MontageObj->MarkPackageDirty();
}

void SMontageEditor::PostAnimUpdate()
{
	SortAndUpdateMontage();
	OnMontageModified();
}

bool SMontageEditor::IsDiffererentFromParent(FName SlotName, int32 SegmentIdx, const FAnimSegment& Segment)
{
	// if it doesn't hare parent asset, no reason to come here
	if (MontageObj && ensureAlways(MontageObj->ParentAsset))
	{
		// find correct source asset from parent
		UAnimMontage* ParentMontage = Cast<UAnimMontage>(MontageObj->ParentAsset);
		if (ParentMontage->IsValidSlot(SlotName))
		{
			const FAnimTrack* ParentTrack = ParentMontage->GetAnimationData(SlotName);

			if (ParentTrack && ParentTrack->AnimSegments.IsValidIndex(SegmentIdx))
			{
				UAnimSequenceBase* SourceAsset = ParentTrack->AnimSegments[SegmentIdx].AnimReference;
				return (SourceAsset != Segment.AnimReference);
			}
		}
	}

	// if something doesn't match, we assume they're different, so default feedback  is to return true
	return true;
}

void SMontageEditor::ReplaceAnimationMapping(FName SlotName, int32 SegmentIdx, UAnimSequenceBase* OldSequenceBase, UAnimSequenceBase* NewSequenceBase)
{
	// if it doesn't hare parent asset, no reason to come here
	if (MontageObj && ensureAlways(MontageObj->ParentAsset))
	{
		// find correct source asset from parent
		UAnimMontage* ParentMontage = Cast<UAnimMontage>(MontageObj->ParentAsset);
		if (ParentMontage->IsValidSlot(SlotName))
		{
			const FAnimTrack* ParentTrack = ParentMontage->GetAnimationData(SlotName);

			if (ParentTrack && ParentTrack->AnimSegments.IsValidIndex(SegmentIdx))
			{
				UAnimSequenceBase* SourceAsset = ParentTrack->AnimSegments[SegmentIdx].AnimReference;
				if (MontageObj->RemapAsset(SourceAsset, NewSequenceBase))
				{
					// success
					return;
				}
			}
		}
	}

	// failed to do the process, check if the animation is correct or if the same type of animation
	// print error
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToRemap", "Make sure the target animation is valid. If source is additive, target animation has to be additive also."));
}

void SMontageEditor::RebuildMontagePanel(bool bNotifyAsset /*= true*/)
{
	SortAndUpdateMontage();
	AnimMontageSectionsPanel->Update();

	if (bNotifyAsset)
	{
		OnMontageModified();
	}
}

EActiveTimerReturnType SMontageEditor::TriggerRebuildMontagePanel(double InCurrentTime, float InDeltaTime)
{
	RebuildMontagePanel();

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SMontageEditor::OnMontageChange(class UObject *EditorAnimBaseObj, bool Rebuild)
{
	bDragging = false;

	if ( MontageObj != nullptr )
	{
		float PreviouewSeqLength = GetSequenceLength();

		if(Rebuild && !bIsActiveTimerRegistered)
		{
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SMontageEditor::TriggerRebuildMontagePanel));
		} 
		else
		{
			CollapseMontage();
		}

		// if animation length changed, we might be out of range, let's restart
		if (GetSequenceLength() != PreviouewSeqLength)
		{
			// this might not be safe
			RestartPreview();
		}

		OnMontageModified();
	}
}

void SMontageEditor::InitDetailsViewEditorObject(UEditorAnimBaseObj* EdObj)
{
	EdObj->InitFromAnim(MontageObj, FOnAnimObjectChange::CreateSP( SharedThis(this), &SMontageEditor::OnMontageChange ));
}

/** This will remove empty spaces in the montage's anim segment but not resort. e.g. - all cached indexes remains valid. UI IS NOT REBUILT after this */
void SMontageEditor::CollapseMontage()
{
	if (MontageObj==NULL)
	{
		return;
	}

	for (int32 i=0; i < MontageObj->SlotAnimTracks.Num(); i++)
	{
		MontageObj->SlotAnimTracks[i].AnimTrack.CollapseAnimSegments();
	}

	MontageObj->UpdateLinkableElements();

	RecalculateSequenceLength();
}

/** This will sort all components of the montage and update (recreate) the UI */
void SMontageEditor::SortAndUpdateMontage()
{
	if (MontageObj==NULL)
	{
		return;
	}
	
	SortAnimSegments();

	MontageObj->UpdateLinkableElements();

	RecalculateSequenceLength();

	SortSections();

	RefreshNotifyTriggerOffsets();

	// Update view (this will recreate everything)
	AnimMontagePanel->Update();
	AnimMontageSectionsPanel->Update();
	AnimTimingPanel->Update();

	// Restart the preview instance of the montage
	RestartPreview();
}

float SMontageEditor::CalculateSequenceLengthOfEditorObject() const
{
	return MontageObj->CalculateSequenceLength();
}

/** Sort Segments by starting time */
void SMontageEditor::SortAnimSegments()
{
	for (int32 I=0; I < MontageObj->SlotAnimTracks.Num(); I++)
	{
		MontageObj->SlotAnimTracks[I].AnimTrack.SortAnimSegments();
	}
}

/** Sort Composite Sections by Start TIme */
void SMontageEditor::SortSections()
{
	struct FCompareSections
	{
		bool operator()( const FCompositeSection &A, const FCompositeSection &B ) const
		{
			return A.GetTime() < B.GetTime();
		}
	};
	if (MontageObj != NULL)
	{
		MontageObj->CompositeSections.Sort(FCompareSections());
	}

	EnsureStartingSection();
}

/** Ensure there is at least one section in the montage and that the first section starts at T=0.f */
void SMontageEditor::EnsureStartingSection()
{
	if (UAnimMontageFactory::EnsureStartingSection(MontageObj))
	{
		OnMontageModified();
	}
}

/** Esnure there is at least one slot node track */
void SMontageEditor::EnsureSlotNode()
{
	if (MontageObj && MontageObj->SlotAnimTracks.Num()==0)
	{
		AddNewMontageSlot(FAnimSlotGroup::DefaultSlotName);
		OnMontageModified();
	}
}

/** Make sure all Sections and Notifies are clamped to NewEndTime (called before NewEndTime is set to SequenceLength) */
bool SMontageEditor::ClampToEndTime(float NewEndTime)
{
	bool bClampingNeeded = SAnimEditorBase::ClampToEndTime(NewEndTime);
	if(bClampingNeeded)
	{
		float ratio = NewEndTime / MontageObj->SequenceLength;

		for(int32 i=0; i < MontageObj->CompositeSections.Num(); i++)
		{
			if(MontageObj->CompositeSections[i].GetTime() > NewEndTime)
			{
				float CurrentTime = MontageObj->CompositeSections[i].GetTime();
				MontageObj->CompositeSections[i].SetTime(CurrentTime * ratio);
			}
		}

		for(int32 i=0; i < MontageObj->Notifies.Num(); i++)
		{
			FAnimNotifyEvent& Notify = MontageObj->Notifies[i];
			float NotifyTime = Notify.GetTime();

			if(NotifyTime >= NewEndTime)
			{
				Notify.SetTime(NotifyTime * ratio);
				Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(MontageObj->CalculateOffsetForNotify(Notify.GetTime()));
			}
		}
	}

	return bClampingNeeded;
}

void SMontageEditor::AddNewSection(float StartTime, FString SectionName)
{
	if ( MontageObj != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT("AddNewSection", "Add New Section") );
		MontageObj->Modify();

		if (MontageObj->AddAnimCompositeSection(FName(*SectionName), StartTime) != INDEX_NONE)
		{
			RebuildMontagePanel();
		}
		OnMontageModified();
	}
}

void SMontageEditor::RemoveSection(int32 SectionIndex)
{
	if(ValidSection(SectionIndex))
	{
		const FScopedTransaction Transaction( LOCTEXT("DeleteSection", "Delete Section") );
		MontageObj->Modify();

		MontageObj->CompositeSections.RemoveAt(SectionIndex);
		EnsureStartingSection();
		OnMontageModified();
		AnimMontageSectionsPanel->Update();
		AnimTimingPanel->Update();
		RestartPreview();
	}
}

FString	SMontageEditor::GetSectionName(int32 SectionIndex) const
{
	if (ValidSection(SectionIndex))
	{
		return MontageObj->GetSectionName(SectionIndex).ToString();
	}
	return FString();
}

void SMontageEditor::RenameSlotNode(int32 SlotIndex, FString NewSlotName)
{
	if(MontageObj->SlotAnimTracks.IsValidIndex(SlotIndex))
	{
		FName NewName(*NewSlotName);
		if(MontageObj->SlotAnimTracks[SlotIndex].SlotName != NewName)
		{
			const FScopedTransaction Transaction( LOCTEXT("RenameSlot", "Rename Slot") );
			MontageObj->Modify();

			MontageObj->SlotAnimTracks[SlotIndex].SlotName = NewName;
			OnMontageModified();
		}
	}
}

void SMontageEditor::AddNewMontageSlot( FName NewSlotName )
{
	if ( MontageObj != nullptr )
	{
		const FScopedTransaction Transaction( LOCTEXT("AddSlot", "Add Slot") );
		MontageObj->Modify();

		MontageObj->AddSlot(NewSlotName);

		OnMontageModified();

		if (AnimMontagePanel.IsValid())
		{
			AnimMontagePanel->Update();
		}
	}
}

FText SMontageEditor::GetMontageSlotName(int32 SlotIndex) const
{
	if(MontageObj->SlotAnimTracks.IsValidIndex(SlotIndex) && MontageObj->SlotAnimTracks[SlotIndex].SlotName != NAME_None)
	{
		return FText::FromName( MontageObj->SlotAnimTracks[SlotIndex].SlotName );
	}	
	return FText::GetEmpty();
}

void SMontageEditor::RemoveMontageSlot(int32 AnimSlotIndex)
{
	if ( MontageObj != nullptr && MontageObj->SlotAnimTracks.IsValidIndex( AnimSlotIndex ) )
	{
		const FScopedTransaction Transaction( LOCTEXT("RemoveSlot", "Remove Slot") );
		MontageObj->Modify();

		MontageObj->SlotAnimTracks.RemoveAt(AnimSlotIndex);
		OnMontageModified();
		AnimMontagePanel->Update();

		// Iterate the notifies and relink anything that is now invalid
		for(FAnimNotifyEvent& Event : MontageObj->Notifies)
		{
			Event.ConditionalRelink();
		}

		// Do the same for sections
		for(FCompositeSection& Section : MontageObj->CompositeSections)
		{
			Section.ConditionalRelink();
		}
	}
}

bool SMontageEditor::CanRemoveMontageSlot(int32 AnimSlotIndex)
{
	return (MontageObj != nullptr) && (MontageObj->SlotAnimTracks.Num()) > 1;
}

void SMontageEditor::DuplicateMontageSlot(int32 AnimSlotIndex)
{
	if (MontageObj != nullptr && MontageObj->SlotAnimTracks.IsValidIndex(AnimSlotIndex))
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateSlot", "Duplicate Slot"));
		MontageObj->Modify();

		FSlotAnimationTrack& NewTrack = MontageObj->AddSlot(FAnimSlotGroup::DefaultSlotName); 
		NewTrack.AnimTrack = MontageObj->SlotAnimTracks[AnimSlotIndex].AnimTrack;

		OnMontageModified();

		AnimMontagePanel->Update();
	}
}

void SMontageEditor::ShowSectionInDetailsView(int32 SectionIndex)
{
	UEditorCompositeSection *Obj = Cast<UEditorCompositeSection>(ShowInDetailsView(UEditorCompositeSection::StaticClass()));
	if ( Obj != nullptr )
	{
		Obj->InitSection(SectionIndex);
	}
	RestartPreviewFromSection(SectionIndex);
}

void SMontageEditor::RestartPreview()
{
	if (UDebugSkelMeshComponent* MeshComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		if (UAnimPreviewInstance* Preview = MeshComponent->PreviewInstance)
		{
			Preview->MontagePreview_PreviewNormal(INDEX_NONE, Preview->IsPlaying());
		}
	}
}

void SMontageEditor::RestartPreviewFromSection(int32 FromSectionIdx)
{
	if (UDebugSkelMeshComponent* MeshComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		if (UAnimPreviewInstance* Preview = MeshComponent->PreviewInstance)
		{
			Preview->MontagePreview_PreviewNormal(FromSectionIdx, Preview->IsPlaying());
		}
	}
}

void SMontageEditor::RestartPreviewPlayAllSections()
{
	if (UDebugSkelMeshComponent* MeshComponent = GetPreviewScene()->GetPreviewMeshComponent())
	{
		if (UAnimPreviewInstance* Preview = MeshComponent->PreviewInstance)
		{
			Preview->MontagePreview_PreviewAllSections(Preview->IsPlaying());
		}
	}
}

void SMontageEditor::MakeDefaultSequentialSections()
{
	check( MontageObj != nullptr );
	SortSections();
	for(int32 SectionIdx=0; SectionIdx < MontageObj->CompositeSections.Num(); SectionIdx++)
	{
		MontageObj->CompositeSections[SectionIdx].NextSectionName = MontageObj->CompositeSections.IsValidIndex(SectionIdx+1) ? MontageObj->CompositeSections[SectionIdx+1].SectionName : NAME_None;
	}
	RestartPreview();
}

void SMontageEditor::ClearSquenceOrdering()
{
	check( MontageObj != nullptr );
	SortSections();
	for(int32 SectionIdx=0; SectionIdx < MontageObj->CompositeSections.Num(); SectionIdx++)
	{
		MontageObj->CompositeSections[SectionIdx].NextSectionName = NAME_None;
	}
	RestartPreview();
}

void SMontageEditor::PostUndo()
{
	// when undo or redo happens, we still have to recalculate length, so we can't rely on sequence length changes or not
	if (MontageObj->SequenceLength)
	{
		MontageObj->SequenceLength = 0.f;
	}

	RebuildMontagePanel(); //Rebuild here, undoing adds can cause slate to crash later on if we don't (using dummy args since they aren't used by the method
}
#undef LOCTEXT_NAMESPACE

