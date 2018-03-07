// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationCompressionPanel.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "AnimationUtils.h"
#include "Animation/AnimCompress.h"
#include "AnimationEditorUtils.h"

#include "Editor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "AnimationCompression"

UCompressionHolder::UCompressionHolder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Compression = FAnimationUtils::GetDefaultAnimationCompressionAlgorithm();
}

//////////////////////////////////////////////
//	FDlgAnimCompression

FDlgAnimCompression::FDlgAnimCompression( TArray<TWeakObjectPtr<UAnimSequence>>& AnimSequences )
{
	if (FSlateApplication::IsInitialized())
	{
		DialogWindow = SNew(SWindow)
			.Title( LOCTEXT("AnimCompression", "Animation Compression") )
			.SupportsMinimize(false) .SupportsMaximize(false)
			.SizingRule( ESizingRule::UserSized )
			.ClientSize(FVector2D(400, 500));

		TSharedPtr<SBorder> DialogWrapper = 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SAssignNew(DialogWidget, SAnimationCompressionPanel)
				.AnimSequences(AnimSequences)
				.ParentWindow(DialogWindow)
			];

		DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	}
}

void FDlgAnimCompression::ShowModal()
{
	GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
}

void SAnimationCompressionPanel::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow.Get();

	AnimSequences = InArgs._AnimSequences;
	CompressionHolder = NewObject<UCompressionHolder>();
	CompressionHolder->AddToRoot();

	if (AnimSequences.Num() == 1)
	{
		UAnimSequence* Seq = AnimSequences[0].Get();

		CompressionHolder->Compression = static_cast<UAnimCompress*>(StaticDuplicateObject(Seq->CompressionScheme, CompressionHolder));
	}

	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	const bool bAllowSearch = true;
	FDetailsViewArgs DetailsViewArgs(/*bUpdateFromSelection=*/ false, /*bLockable=*/ false, bAllowSearch, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true);

	TSharedPtr<class IDetailsView> PropertyView = EditModule.CreateDetailView(DetailsViewArgs);

	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(CompressionHolder);
	PropertyView->SetObjects(SelectedObjects);

	TSharedRef<SVerticalBox> Box =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(8.0f, 4.0f, 8.0f, 4.0f)
		[
			PropertyView.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f, 4.0f, 8.0f)
		[
			SNew(SSeparator)
		]
		+SVerticalBox::Slot()
		.Padding(4.0f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.AutoHeight()
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
			.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
			.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
			+ SUniformGridPanel::Slot(0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("AnimCompressionApply", "Apply"))
			.HAlign(HAlign_Center)
			.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
			.OnClicked(this, &SAnimationCompressionPanel::ApplyClicked)
			]
		];
	
	this->ChildSlot[Box];
}

SAnimationCompressionPanel::~SAnimationCompressionPanel()
{
	if (CompressionHolder)
	{
		CompressionHolder->Compression = nullptr;
		CompressionHolder->RemoveFromRoot();
	}
}

FReply SAnimationCompressionPanel::ApplyClicked()
{
	ApplyAlgorithm( CompressionHolder->Compression );
	return FReply::Handled();
}

void SAnimationCompressionPanel::ApplyAlgorithm(class UAnimCompress* Algorithm)
{
	if ( Algorithm )
	{
		TArray<UAnimSequence*> AnimSequencePtrs;
		AnimSequencePtrs.Reserve(AnimSequences.Num());

		for(int32 Index = 0; Index < AnimSequences.Num(); ++Index)
		{
			AnimSequencePtrs.Add(AnimSequences[Index].Get());
		}

		if (AnimationEditorUtils::ApplyCompressionAlgorithm(AnimSequencePtrs, Algorithm))
		{
			TSharedPtr<SWindow> Win = ParentWindow.Pin();
			if (Win.IsValid())
			{
				Win->RequestDestroyWindow();
			}
		}
	}
}
#undef LOCTEXT_NAMESPACE
