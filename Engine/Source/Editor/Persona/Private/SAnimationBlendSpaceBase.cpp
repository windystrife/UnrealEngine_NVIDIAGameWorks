// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimationBlendSpaceBase.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "SlateOptMacros.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ScopedTransaction.h"
#include "AnimPreviewInstance.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "SAnimationBlendSpaceGridWidget.h"

#define LOCTEXT_NAMESPACE "BlendSpaceEditorBase"

SBlendSpaceEditorBase::~SBlendSpaceEditorBase()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);
}

void SBlendSpaceEditorBase::Construct(const FArguments& InArgs, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& OnPostUndo)
{
	BlendSpace = InArgs._BlendSpace;
	PreviewScenePtr = InPreviewScene;
	OnPostUndo.Add(FSimpleDelegate::CreateSP( this, &SBlendSpaceEditorBase::PostUndo ) );

	bShouldSetPreviewValue = false;

	SAnimEditorBase::Construct(SAnimEditorBase::FArguments().DisplayAnimInfoBar(false), InPreviewScene);

	NonScrollEditorPanels->AddSlot()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.Padding(2) 
					[
						SNew(SVerticalBox)
						// Grid area
						+SVerticalBox::Slot()
						.FillHeight(1)
						[
							SAssignNew(NewBlendSpaceGridWidget, SBlendSpaceGridWidget)
							.Cursor(EMouseCursor::Crosshairs)
							.BlendSpaceBase(BlendSpace)
							.NotifyHook(this)
							.OnSampleMoved(this, &SBlendSpaceEditorBase::OnSampleMoved)
							.OnSampleRemoved(this, &SBlendSpaceEditorBase::OnSampleRemoved)
							.OnSampleAdded(this, &SBlendSpaceEditorBase::OnSampleAdded)
							.OnSampleAnimationChanged(this, &SBlendSpaceEditorBase::OnUpdateAnimation)
						]
					]
				]
			]
		]
	];

	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &SBlendSpaceEditorBase::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);
}

void SBlendSpaceEditorBase::OnSampleMoved(const int32 SampleIndex, const FVector& NewValue, bool bIsInteractive)
{
	bool bMoveSuccesful = true;
	if (BlendSpace->IsValidBlendSampleIndex(SampleIndex) && BlendSpace->GetBlendSample(SampleIndex).SampleValue != NewValue && !BlendSpace->IsTooCloseToExistingSamplePoint(NewValue, SampleIndex))
	{
		FScopedTransaction ScopedTransaction(LOCTEXT("MoveSample", "Moving Blend Grid Sample"));
		BlendSpace->Modify();

		bMoveSuccesful = BlendSpace->EditSampleValue(SampleIndex, NewValue);
		if (bMoveSuccesful)
		{
			BlendSpace->ValidateSampleData();
			ResampleData();
		}
	}
  }

void SBlendSpaceEditorBase::OnSampleRemoved(const int32 SampleIndex)
{
	FScopedTransaction ScopedTransaction(LOCTEXT("RemoveSample", "Removing Blend Grid Sample"));
	BlendSpace->Modify();

	const bool bRemoveSuccesful = BlendSpace->DeleteSample(SampleIndex);
	if (bRemoveSuccesful)
	{
		ResampleData();
		BlendSpace->ValidateSampleData();
	}
	BlendSpace->PostEditChange();
}

void SBlendSpaceEditorBase::OnSampleAdded(UAnimSequence* Animation, const FVector& Value)
{
	FScopedTransaction ScopedTransaction(LOCTEXT("AddSample", "Adding Blend Grid Sample"));
	BlendSpace->Modify();

	const bool bAddSuccesful = BlendSpace->AddSample(Animation, Value);	
	if (bAddSuccesful)
	{
		ResampleData();
		BlendSpace->ValidateSampleData();
	}
	BlendSpace->PostEditChange();
}

void SBlendSpaceEditorBase::OnUpdateAnimation(UAnimSequence* Animation, const FVector& Value)
{
	FScopedTransaction ScopedTransaction(LOCTEXT("UpdateAnimation", "Changing Animation Sequence"));
	BlendSpace->Modify();

	const bool bUpdateSuccesful = BlendSpace->UpdateSampleAnimation(Animation, Value);
	if (bUpdateSuccesful)
	{
		ResampleData();
		BlendSpace->ValidateSampleData();
	}
}

void SBlendSpaceEditorBase::PostUndo()
{
	// Validate and resample blend space data
	BlendSpace->ValidateSampleData();
	ResampleData();

	// Invalidate widget data
	NewBlendSpaceGridWidget->InvalidateCachedData();

	// Invalidate sample indices used for UI info
	NewBlendSpaceGridWidget->InvalidateState();

	// Set flag which will update the preview value in the next tick (this due the recreation of data after Undo)
	bShouldSetPreviewValue = true;
}

void SBlendSpaceEditorBase::UpdatePreviewParameter() const
{
	class UDebugSkelMeshComponent* Component = GetPreviewScene()->GetPreviewMeshComponent();

	if (Component != nullptr && Component->IsPreviewOn())
	{
		if (Component->PreviewInstance->GetCurrentAsset() == BlendSpace)
		{
			const FVector BlendInput = NewBlendSpaceGridWidget->GetBlendPreviewValue();
			Component->PreviewInstance->SetBlendSpaceInput(BlendInput);
			GetPreviewScene()->InvalidateViews();			
		}
	}
}

TSharedRef<class IPersonaPreviewScene> SBlendSpaceEditorBase::GetPreviewScene() const
{
	return PreviewScenePtr.Pin().ToSharedRef();
}

void SBlendSpaceEditorBase::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Update the preview as long as its enabled
	if (NewBlendSpaceGridWidget->IsPreviewing() || bShouldSetPreviewValue)
	{
		UpdatePreviewParameter();
		bShouldSetPreviewValue = false;
	}
}

void SBlendSpaceEditorBase::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (ObjectBeingModified == BlendSpace)
	{
		BlendSpace->ValidateSampleData();
		ResampleData();
		NewBlendSpaceGridWidget->InvalidateCachedData();
	}
}

void SBlendSpaceEditorBase::NotifyPreChange(UProperty* PropertyAboutToChange)
{
	if (BlendSpace)
	{
		BlendSpace->Modify();
	}	
}

void SBlendSpaceEditorBase::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	if (BlendSpace)
	{
		BlendSpace->ValidateSampleData();
		ResampleData();
		BlendSpace->MarkPackageDirty();
	}
}

#undef LOCTEXT_NAMESPACE
