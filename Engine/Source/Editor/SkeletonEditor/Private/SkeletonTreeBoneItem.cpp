// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeBoneItem.h"
#include "SSkeletonTreeRow.h"
#include "IPersonaPreviewScene.h"
#include "IDocumentation.h"
#include "Animation/BlendProfile.h"
#include "IEditableSkeleton.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "BoneDragDropOp.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "SocketDragDropOp.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SToolTip.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SkeletalMeshTypes.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FSkeletonTreeBoneItem"

FSkeletonTreeBoneItem::FSkeletonTreeBoneItem(const FName& InBoneName, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
	: FSkeletonTreeItem(InSkeletonTree)
	, BoneName(InBoneName)
	, bWeightedBone(false)
	, bRequiredBone(false)
{
	static const FString BoneProxyPrefix(TEXT("BONEPROXY_"));

	BoneProxy = NewObject<UBoneProxy>(GetTransientPackage(), *(BoneProxyPrefix + FString::Printf(TEXT("%p"), &InSkeletonTree.Get()) + InBoneName.ToString()));
	BoneProxy->SetFlags(RF_Transactional);
	BoneProxy->BoneName = InBoneName;
	TSharedPtr<IPersonaPreviewScene> PreviewScene = InSkeletonTree->GetPreviewScene();
	if (PreviewScene.IsValid())
	{
		BoneProxy->SkelMeshComponent = PreviewScene->GetPreviewMeshComponent();
	}
}

EVisibility FSkeletonTreeBoneItem::GetLODIconVisibility() const
{
	if (bRequiredBone)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

void FSkeletonTreeBoneItem::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected )
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
			.Visibility(this, &FSkeletonTreeBoneItem::GetLODIconVisibility)
		];

	if (GetSkeletonTree()->GetPreviewScene().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComponent = GetSkeletonTree()->GetPreviewScene()->GetPreviewMeshComponent();
		CacheLODChange(PreviewComponent);
	}	
	
	FText ToolTip = GetBoneToolTip();
	Box->AddSlot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			SNew( STextBlock )
			.ColorAndOpacity(this, &FSkeletonTreeBoneItem::GetBoneTextColor)
			.Text( FText::FromName(BoneName) )
			.HighlightText( FilterText )
			.Font(this, &FSkeletonTreeBoneItem::GetBoneTextFont)
			.ToolTipText( ToolTip )
		];
}

TSharedRef< SWidget > FSkeletonTreeBoneItem::GenerateWidgetForDataColumn(const FName& DataColumnName)
{
	if(DataColumnName == ISkeletonTree::Columns::Retargeting)
	{
		return 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SAssignNew(RetargetingComboButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
				.ForegroundColor(this, &FSkeletonTreeBoneItem::GetRetargetingComboButtonForegroundColor)
				.ContentPadding(0)
				.OnGetMenuContent(this, &FSkeletonTreeBoneItem::CreateBoneTranslationRetargetingModeMenu)
				.ToolTip(IDocumentation::Get()->CreateToolTip(
				LOCTEXT("RetargetingToolTip", "Set bone translation retargeting mode"),
				nullptr,
				TEXT("Shared/Editors/Persona"),
				TEXT("TranslationRetargeting")))
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &FSkeletonTreeBoneItem::GetTranslationRetargetingModeMenuTitle)
					]
				]
			];
	}
	else if(DataColumnName == ISkeletonTree::Columns::BlendProfile)
	{
		bool bWritable = true;

		UBlendProfile* CurrentProfile = GetSkeletonTree()->GetSelectedBlendProfile();

		// We should never have this column if we don't have a profile
		check(CurrentProfile);

		return SNew(SBox)
			.Padding(0.0f)
			.HAlign(HAlign_Left)
			[
				SNew(SSpinBox<float>)
				.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("SkeletonTree.HyperlinkSpinBox"))
				.ContentPadding(0.0f)
				.MinValue(0.0f)
				.MaxValue(1000.0f)
				.Value(CurrentProfile->GetBoneBlendScale(BoneName))
				.OnValueCommitted(this, &FSkeletonTreeBoneItem::OnBlendSliderCommitted)
			];
	}

	return SNullWidget::NullWidget;
}

FSlateColor FSkeletonTreeBoneItem::GetRetargetingComboButtonForegroundColor() const
{
	static const FName InvertedForegroundName("InvertedForeground");
	static const FName DefaultForegroundName("DefaultForeground");

	if (RetargetingComboButton.IsValid())
	{
		return RetargetingComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
	}
	return FSlateColor::UseForeground();
}

TSharedRef< SWidget > FSkeletonTreeBoneItem::CreateBoneTranslationRetargetingModeMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("BoneTranslationRetargetingMode", LOCTEXT( "BoneTranslationRetargetingModeMenuHeading", "Bone Translation Retargeting Mode" ) );
	{
		UEnum* const Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EBoneTranslationRetargetingMode"), true);	
		check(Enum);

		FUIAction ActionRetargetingAnimation = FUIAction(FExecuteAction::CreateSP(this, &FSkeletonTreeBoneItem::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::Animation));
		MenuBuilder.AddMenuEntry( Enum->GetDisplayNameTextByValue(EBoneTranslationRetargetingMode::Animation), LOCTEXT( "BoneTranslationRetargetingAnimationToolTip", "Use translation from animation." ), FSlateIcon(), ActionRetargetingAnimation);

		FUIAction ActionRetargetingSkeleton = FUIAction(FExecuteAction::CreateSP(this, &FSkeletonTreeBoneItem::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::Skeleton));
		MenuBuilder.AddMenuEntry( Enum->GetDisplayNameTextByValue(EBoneTranslationRetargetingMode::Skeleton), LOCTEXT( "BoneTranslationRetargetingSkeletonToolTip", "Use translation from Skeleton." ), FSlateIcon(), ActionRetargetingSkeleton);

		FUIAction ActionRetargetingLengthScale = FUIAction(FExecuteAction::CreateSP(this, &FSkeletonTreeBoneItem::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::AnimationScaled));
		MenuBuilder.AddMenuEntry( Enum->GetDisplayNameTextByValue(EBoneTranslationRetargetingMode::AnimationScaled), LOCTEXT( "BoneTranslationRetargetingAnimationScaledToolTip", "Use translation from animation, scale length by Skeleton's proportions." ), FSlateIcon(), ActionRetargetingLengthScale);

		FUIAction ActionRetargetingAnimationRelative = FUIAction(FExecuteAction::CreateSP(this, &FSkeletonTreeBoneItem::SetBoneTranslationRetargetingMode, EBoneTranslationRetargetingMode::AnimationRelative));
		MenuBuilder.AddMenuEntry( Enum->GetDisplayNameTextByValue(EBoneTranslationRetargetingMode::AnimationRelative), LOCTEXT("BoneTranslationRetargetingAnimationRelativeToolTip", "Use relative translation from animation similar to an additive animation."), FSlateIcon(), ActionRetargetingAnimationRelative);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText FSkeletonTreeBoneItem::GetTranslationRetargetingModeMenuTitle() const
{
	const USkeleton& Skeleton = GetEditableSkeleton()->GetSkeleton();

	const int32 BoneIndex = Skeleton.GetReferenceSkeleton().FindBoneIndex( BoneName );
	if( BoneIndex != INDEX_NONE )
	{
		const EBoneTranslationRetargetingMode::Type RetargetingMode = Skeleton.GetBoneTranslationRetargetingMode(BoneIndex);
		UEnum* const Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EBoneTranslationRetargetingMode"), true);	
		if (Enum)
		{
			return Enum->GetDisplayNameTextByValue(RetargetingMode);
		}
	}

	return LOCTEXT("None", "None");
}

void FSkeletonTreeBoneItem::SetBoneTranslationRetargetingMode(EBoneTranslationRetargetingMode::Type NewRetargetingMode)
{
	GetEditableSkeleton()->SetBoneTranslationRetargetingMode(BoneName, NewRetargetingMode);
}

void FSkeletonTreeBoneItem::SetBoneBlendProfileScale(float NewScale, bool bRecurse)
{
	FName BlendProfileName = GetSkeletonTree()->GetSelectedBlendProfile()->GetFName();
	GetEditableSkeleton()->SetBlendProfileScale(BlendProfileName, BoneName, NewScale, bRecurse);
}

FSlateFontInfo FSkeletonTreeBoneItem::GetBoneTextFont() const
{
	if (bWeightedBone)
	{
		return FEditorStyle::GetWidgetStyle<FTextBlockStyle>("SkeletonTree.BoldFont").Font;
	}
	else
	{
		return FEditorStyle::GetWidgetStyle<FTextBlockStyle>("SkeletonTree.NormalFont").Font;
	}
}

void FSkeletonTreeBoneItem::CacheLODChange(UDebugSkelMeshComponent* PreviewComponent)
{
	bWeightedBone = false;
	bRequiredBone = false;
	if (PreviewComponent)
	{
		int32 BoneIndex = PreviewComponent->GetBoneIndex(BoneName);

		if (BoneIndex != INDEX_NONE)
		{
			if (IsBoneWeighted(BoneIndex, PreviewComponent))
			{
				//Bone is vertex weighted
				bWeightedBone = true;
			}
			if (IsBoneRequired(BoneIndex, PreviewComponent))
			{
				bRequiredBone = true;
			}
		}
	}
}

void FSkeletonTreeBoneItem::EnableBoneProxyTick(bool bEnable)
{
	BoneProxy->bIsTickable = bEnable;
}

FSlateColor FSkeletonTreeBoneItem::GetBoneTextColor() const
{
	if (FilterResult == ESkeletonTreeFilterResult::ShownDescendant)
	{
		return FSlateColor(FLinearColor::Gray * 0.5f);
	}
	else if (bRequiredBone)
	{
		return FSlateColor(FLinearColor::White);
	}
	else
	{
		return FSlateColor(FLinearColor::Gray);
	}
}

FReply FSkeletonTreeBoneItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		return FReply::Handled().BeginDragDrop( FBoneDragDropOp::New(GetEditableSkeleton(), BoneName ) );
	}

	return FReply::Unhandled();
}

FText FSkeletonTreeBoneItem::GetBoneToolTip()
{
	bool bIsMeshBone = false;
	bool bIsWeightedBone = false;
	bool bMeshExists = false;

	FText ToolTip;

	if (GetSkeletonTree()->GetPreviewScene().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComponent = GetSkeletonTree()->GetPreviewScene()->GetPreviewMeshComponent();

		if (PreviewComponent)
		{
			bMeshExists = true;

			int32 BoneIndex = PreviewComponent->GetBoneIndex(BoneName);

			if (BoneIndex != INDEX_NONE)
			{
				bIsMeshBone = true;

				bIsWeightedBone = IsBoneWeighted(BoneIndex, PreviewComponent);
			}
		}
	}

	if ( !bMeshExists )
	{
		ToolTip = LOCTEXT( "BoneToolTipNoMeshAvailable", "This bone exists only on the skeleton as there is no current mesh set" );
	}
	else
	{
		if ( !bIsMeshBone )
		{
			ToolTip = LOCTEXT( "BoneToolTipSkeletonOnly", "This bone exists only on the skeleton, but not on the current mesh" );
		}
		else
		{
			if ( !bIsWeightedBone )
			{
				ToolTip = LOCTEXT( "BoneToolTipSkeletonAndMesh", "This bone is used by the current mesh, but has no vertices weighted against it" );
			}
			else
			{
				ToolTip = LOCTEXT( "BoneToolTipWeighted", "This bone has vertices weighted against it" );
			}
		}
	}

	return ToolTip;
}

void FSkeletonTreeBoneItem::OnBlendSliderCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if(CommitType == ETextCommit::OnEnter)
	{
		SetBoneBlendProfileScale(NewValue, false);
	}
}

void FSkeletonTreeBoneItem::HandleDragEnter(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();

	// Is someone dragging a socket onto a bone?
	if (DragConnectionOp.IsValid())
	{
		if (BoneName != DragConnectionOp->GetSocketInfo().Socket->BoneName)
		{
			// The socket can be dropped here if we're a bone and NOT the socket's existing parent
			DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Ok" ) ) );
		}
		else if (DragConnectionOp->IsAltDrag())
		{
			// For Alt-Drag, dropping onto the existing parent is fine, as we're going to copy, not move the socket
			DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Ok" ) ) );
		}
	}
}

void FSkeletonTreeBoneItem::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();
	if (DragConnectionOp.IsValid())
	{
		// Reset the drag/drop icon when leaving this row
		DragConnectionOp->SetIcon( FEditorStyle::GetBrush( TEXT( "Graph.ConnectorFeedback.Error" ) ) );
	}
}

FReply FSkeletonTreeBoneItem::HandleDrop(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FSocketDragDropOp> DragConnectionOp = DragDropEvent.GetOperationAs<FSocketDragDropOp>();
	if (DragConnectionOp.IsValid())
	{
		FSelectedSocketInfo SocketInfo = DragConnectionOp->GetSocketInfo();

		if (DragConnectionOp->IsAltDrag())
		{
			// In an alt-drag, the socket can be dropped on any bone
			// (including its existing parent) to create a uniquely named copy
			GetSkeletonTree()->DuplicateAndSelectSocket( SocketInfo, BoneName);
		}
		else if (BoneName != SocketInfo.Socket->BoneName)
		{
			// The socket can be dropped here if we're a bone and NOT the socket's existing parent
			USkeletalMesh* SkeletalMesh = GetSkeletonTree()->GetPreviewScene().IsValid() ? GetSkeletonTree()->GetPreviewScene()->GetPreviewMeshComponent()->SkeletalMesh : nullptr;
			GetEditableSkeleton()->SetSocketParent(SocketInfo.Socket->SocketName, BoneName, SkeletalMesh);

			return FReply::Handled();
		}
	}
	else
	{
		TSharedPtr<FAssetDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
		if (DragDropOp.IsValid())
		{
			//Do we have some assets to attach?
			if (DragDropOp->HasAssets())
			{
				GetSkeletonTree()->AttachAssets(SharedThis(this), DragDropOp->GetAssets());
			}
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool FSkeletonTreeBoneItem::IsBoneWeighted(int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent)
{
	// MeshBoneIndex must be an index into the mesh's skeleton, *not* the source skeleton!!!

	if (!PreviewComponent || !PreviewComponent->SkeletalMesh || !PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num())
	{
		// If there's no mesh, then this bone can't possibly be weighted!
		return false;
	}

	//Get current LOD
	const int32 LODIndex = FMath::Clamp(PreviewComponent->PredictedLODLevel, 0, PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num() - 1);
	FStaticLODModel& LODModel = PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels[LODIndex];

	//Check whether the bone is vertex weighted
	int32 Index = LODModel.ActiveBoneIndices.Find(MeshBoneIndex);

	return Index != INDEX_NONE;
}

bool FSkeletonTreeBoneItem::IsBoneRequired(int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent)
{
	// MeshBoneIndex must be an index into the mesh's skeleton, *not* the source skeleton!!!

	if (!PreviewComponent || !PreviewComponent->SkeletalMesh || !PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num())
	{
		// If there's no mesh, then this bone can't possibly be weighted!
		return false;
	}

	//Get current LOD
	const int32 LODIndex = FMath::Clamp(PreviewComponent->PredictedLODLevel, 0, PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels.Num() - 1);
	FStaticLODModel& LODModel = PreviewComponent->SkeletalMesh->GetImportedResource()->LODModels[LODIndex];

	//Check whether the bone is vertex weighted
	int32 Index = LODModel.RequiredBones.Find(MeshBoneIndex);

	return Index != INDEX_NONE;
}

void FSkeletonTreeBoneItem::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(BoneProxy);
}

#undef LOCTEXT_NAMESPACE