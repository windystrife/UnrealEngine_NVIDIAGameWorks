// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "SkeletonTreeItem.h"
#include "PhysicsEngine/PhysicsAsset.h"

class FSkeletonTreePhysicsBodyItem : public FSkeletonTreeItem
{
public:
	SKELETON_TREE_ITEM_TYPE(FSkeletonTreePhysicsBodyItem, FSkeletonTreeItem)

	FSkeletonTreePhysicsBodyItem(USkeletalBodySetup* InBodySetup, int32 InBodySetupIndex, const FName& InBoneName, bool bInHasBodySetup, bool bInHasShapes, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
		: FSkeletonTreeItem(InSkeletonTree)
		, BodySetup(InBodySetup)
		, BodySetupIndex(InBodySetupIndex)
		, BoneName(InBoneName)
		, bHasBodySetup(bInHasBodySetup)
		, bHasShapes(bInHasShapes)
	{}

	/** ISkeletonTreeItem interface */
	virtual void GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected) override;
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn(const FName& DataColumnName) override;	
	virtual FName GetRowItemName() const override { return BoneName; }
	virtual UObject* GetObject() const override { return BodySetup; }

	/** Get the index of the body setup in the physics asset */
	int32 GetBodySetupIndex() const { return BodySetupIndex; }

private:
	/** Gets the icon to display for this body */
	const FSlateBrush* GetBrush() const;

	/** Gets the color to display the item's text */
	FSlateColor GetBodyTextColor() const;

private:
	/** The body setup we are representing */
	USkeletalBodySetup* BodySetup;

	/** The index of the body setup in the physics asset */
	int32 BodySetupIndex;

	/** The name of the bone that this body is bound to */
	FName BoneName;

	/** Whether there is a body set up for this bone */
	bool bHasBodySetup;

	/** Whether this body has shapes */
	bool bHasShapes;
};
