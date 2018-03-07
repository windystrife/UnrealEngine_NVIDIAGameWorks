// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "ISkeletonTreeItem.h"
#include "SkeletonTreeItem.h"
#include "Animation/Skeleton.h"
#include "Widgets/Input/SComboButton.h"
#include "BoneProxy.h"
#include "GCObject.h"

class UDebugSkelMeshComponent;

class FSkeletonTreeBoneItem : public FSkeletonTreeItem, public FGCObject
{
public:
	SKELETON_TREE_ITEM_TYPE(FSkeletonTreeBoneItem, FSkeletonTreeItem)

	FSkeletonTreeBoneItem(const FName& InBoneName, const TSharedRef<class ISkeletonTree>& InSkeletonTree);

	/** ISkeletonTreeItem interface */
	virtual void GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected) override;
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn(const FName& DataColumnName) override;
	virtual FName GetRowItemName() const override { return BoneName; }
	virtual void HandleDragEnter(const FDragDropEvent& DragDropEvent) override;
	virtual void HandleDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply HandleDrop(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual UObject* GetObject() const override { return BoneProxy; }

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Check to see if the specified bone is weighted in the specified component */
	bool IsBoneWeighted(int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent);

	/** Check to see if the specified bone is required in the specified component */
	bool IsBoneRequired(int32 MeshBoneIndex, UDebugSkelMeshComponent* PreviewComponent);

	/** Cache to use Bold Font or not */
	void CacheLODChange(UDebugSkelMeshComponent* PreviewComponent);

	/** Enable and disable the bone proxy ticking */
	void EnableBoneProxyTick(bool bEnable);

private:
	/** Gets the font for displaying bone text in the skeletal tree */
	FSlateFontInfo GetBoneTextFont() const;

	/** Get the text color based on bone part of skeleton or part of mesh */
	FSlateColor GetBoneTextColor() const;

	/** visibility of the icon */
	EVisibility GetLODIconVisibility() const;

	/** Function that returns the current tooltip for this bone, depending on how it's used by the mesh */
	FText GetBoneToolTip();

	/** Vary the color of the combo based on hover state */
	FSlateColor GetRetargetingComboButtonForegroundColor() const;

	/** Create menu for Bone Translation Retargeting Mode. */
	TSharedRef< SWidget > CreateBoneTranslationRetargetingModeMenu();

	/** Get Title for Bone Translation Retargeting Mode menu. */
	FText GetTranslationRetargetingModeMenuTitle() const;

	/** Callback from a slider widget if the text entry or slider movement is used */
	void OnBlendSliderCommitted(float NewValue, ETextCommit::Type CommitType);

	/** Set Translation Retargeting Mode for this bone. */
	void SetBoneTranslationRetargetingMode(EBoneTranslationRetargetingMode::Type NewRetargetingMode);

	/** Set current Blend Scale for this bone */
	void SetBoneBlendProfileScale(float NewScale, bool bRecurse);

private:
	/** Bone proxy object */
	UBoneProxy* BoneProxy;

	/** The actual bone data that we create Slate widgets to display */
	FName BoneName;

	/** Weighted bone */
	bool bWeightedBone;

	/** Required bone */
	bool bRequiredBone;

	/** Reference to the retargeting combo button */
	TSharedPtr<SComboButton> RetargetingComboButton;

};
