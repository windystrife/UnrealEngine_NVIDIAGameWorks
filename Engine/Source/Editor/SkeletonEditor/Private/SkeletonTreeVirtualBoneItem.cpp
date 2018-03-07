// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeVirtualBoneItem.h"
#include "Widgets/Text/STextBlock.h"
#include "SSkeletonTreeRow.h"
#include "IPersonaPreviewScene.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "SInlineEditableTextBlock.h"
#include "UnrealString.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FSkeletonTreeVirtualBoneItem"

FSkeletonTreeVirtualBoneItem::FSkeletonTreeVirtualBoneItem(const FName& InBoneName, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
	: FSkeletonTreeItem(InSkeletonTree)
	, BoneName(InBoneName)
{
	static const FString BoneProxyPrefix(TEXT("VIRTUALBONEPROXY_"));

	BoneProxy = NewObject<UBoneProxy>(GetTransientPackage(), *(BoneProxyPrefix + FString::Printf(TEXT("%p"), &InSkeletonTree.Get()) + InBoneName.ToString()));
	BoneProxy->SetFlags(RF_Transactional);
	BoneProxy->BoneName = InBoneName;
	TSharedPtr<IPersonaPreviewScene> PreviewScene = InSkeletonTree->GetPreviewScene();
	if (PreviewScene.IsValid())
	{
		BoneProxy->SkelMeshComponent = PreviewScene->GetPreviewMeshComponent();
	}
}

EVisibility FSkeletonTreeVirtualBoneItem::GetLODIconVisibility() const
{
	return EVisibility::Visible;
}

void FSkeletonTreeVirtualBoneItem::GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected)
{
	const FSlateBrush* LODIcon = FEditorStyle::GetBrush("SkeletonTree.LODBone");

	Box->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 1.0f))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(LODIcon)
			.Visibility(this, &FSkeletonTreeVirtualBoneItem::GetLODIconVisibility)
		];

	FText ToolTip = GetBoneToolTip();

	TAttribute<FText> NameAttr = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FSkeletonTreeVirtualBoneItem::GetVirtualBoneNameAsText));

	InlineWidget = SNew(SInlineEditableTextBlock)
						.ColorAndOpacity(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextColor, InIsSelected)
						.Text(NameAttr)
						.HighlightText(FilterText)
						.Font(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextFont)
						.ToolTipText(ToolTip)
						.OnBeginTextEdit(this, &FSkeletonTreeVirtualBoneItem::OnVirtualBoneNameEditing)
						.OnVerifyTextChanged(this, &FSkeletonTreeVirtualBoneItem::OnVerifyBoneNameChanged)
						.OnTextCommitted(this, &FSkeletonTreeVirtualBoneItem::OnCommitVirtualBoneName)
						.IsSelected(InIsSelected);

	OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

	Box->AddSlot()
		.AutoWidth()
		.Padding(2, 2, 1, 0)
		[
			SNew(STextBlock)
			.ColorAndOpacity(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextColor, InIsSelected)
			.Text(FText::FromString(VirtualBoneNameHelpers::VirtualBonePrefix))
			.Font(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextFont)
			.Visibility(this, &FSkeletonTreeVirtualBoneItem::GetVirtualBonePrefixVisibility)
		];

	Box->AddSlot()
		.AutoWidth()
		[
			InlineWidget.ToSharedRef()
		];
}

TSharedRef< SWidget > FSkeletonTreeVirtualBoneItem::GenerateWidgetForDataColumn(const FName& DataColumnName)
{
	return SNullWidget::NullWidget;
}

FSlateFontInfo FSkeletonTreeVirtualBoneItem::GetBoneTextFont() const
{
	return FEditorStyle::GetWidgetStyle<FTextBlockStyle>("SkeletonTree.BoldFont").Font;
}

FSlateColor FSkeletonTreeVirtualBoneItem::GetBoneTextColor(FIsSelected InIsSelected) const
{
	bool bIsSelected = false;
	if (InIsSelected.IsBound())
	{
		bIsSelected = InIsSelected.Execute();
	}

	if(bIsSelected)
	{
		return FSlateColor::UseForeground();
	}
	else
	{
		return FSlateColor(FLinearColor(0.4f, 0.4f, 1.f));
	}
}

FText FSkeletonTreeVirtualBoneItem::GetBoneToolTip()
{
	return LOCTEXT("VirtualBone_ToolTip", "Virtual Bones are added in editor and allow space switching between two different bones in the skeleton.");
}

void FSkeletonTreeVirtualBoneItem::OnItemDoubleClicked()
{
	OnRenameRequested.ExecuteIfBound();
}

void FSkeletonTreeVirtualBoneItem::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

void FSkeletonTreeVirtualBoneItem::OnVirtualBoneNameEditing(const FText& OriginalText)
{
	CachedBoneNameForRename = BoneName;
	BoneName = VirtualBoneNameHelpers::RemoveVirtualBonePrefix(OriginalText.ToString());
}

bool FSkeletonTreeVirtualBoneItem::OnVerifyBoneNameChanged(const FText& InText, FText& OutErrorMessage)
{
	bool bVerifyName = true;

	FString InTextTrimmed = FText::TrimPrecedingAndTrailing(InText).ToString();

	FString NewName = VirtualBoneNameHelpers::AddVirtualBonePrefix(InTextTrimmed);

	if (InTextTrimmed.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyVirtualBoneName_Error", "Virtual bones must have a name!");
		bVerifyName = false;
	}
	else
	{
		if(InTextTrimmed != BoneName.ToString())
		{
			bVerifyName = !GetEditableSkeleton()->DoesVirtualBoneAlreadyExist(NewName);

			// Needs to be checked on verify.
			if (!bVerifyName)
			{

				// Tell the user that the name is a duplicate
				OutErrorMessage = LOCTEXT("DuplicateVirtualBone_Error", "Name in use!");
				bVerifyName = false;
			}
		}
	}

	return bVerifyName;
}

void FSkeletonTreeVirtualBoneItem::OnCommitVirtualBoneName(const FText& InText, ETextCommit::Type CommitInfo)
{
	FString NewNameString = VirtualBoneNameHelpers::AddVirtualBonePrefix(FText::TrimPrecedingAndTrailing(InText).ToString());
	FName NewName(*NewNameString);

	// Notify skeleton tree of rename
	GetEditableSkeleton()->RenameVirtualBone(CachedBoneNameForRename, NewName);
	BoneName = NewName;
}

EVisibility FSkeletonTreeVirtualBoneItem::GetVirtualBonePrefixVisibility() const
{
	return InlineWidget->IsInEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

void FSkeletonTreeVirtualBoneItem::EnableBoneProxyTick(bool bEnable)
{
	BoneProxy->bIsTickable = bEnable;
}

void FSkeletonTreeVirtualBoneItem::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(BoneProxy);
}

#undef LOCTEXT_NAMESPACE
