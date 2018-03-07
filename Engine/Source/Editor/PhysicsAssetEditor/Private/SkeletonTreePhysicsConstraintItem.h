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
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

class FSkeletonTreePhysicsConstraintItem : public FSkeletonTreeItem
{
public:
	SKELETON_TREE_ITEM_TYPE(FSkeletonTreePhysicsConstraintItem, FSkeletonTreeItem)

	FSkeletonTreePhysicsConstraintItem(UPhysicsConstraintTemplate* InConstraint, int32 InConstraintIndex, const FName& InBoneName, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
		: FSkeletonTreeItem(InSkeletonTree)
		, Constraint(InConstraint)
		, ConstraintIndex(InConstraintIndex)
		, BoneName(InBoneName)
	{
		const FConstraintInstance& ConstraintInstance = Constraint->DefaultInstance;
		OtherBoneName = ConstraintInstance.ConstraintBone1 == BoneName ? ConstraintInstance.ConstraintBone2 : ConstraintInstance.ConstraintBone1;
	}

	/** ISkeletonTreeItem interface */
	virtual void GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected) override;
	virtual TSharedRef< SWidget > GenerateWidgetForDataColumn(const FName& DataColumnName) override;	
	virtual FName GetRowItemName() const override { return OtherBoneName; }
	virtual UObject* GetObject() const override { return Constraint; }

	/** Get the index of the constraint in the physics asset */
	int32 GetConstraintIndex() const { return ConstraintIndex; }

private:
	FSlateColor GetConstraintTextColor() const;

private:
	/** The constraint we are representing */
	UPhysicsConstraintTemplate* Constraint;

	/** The index of the body setup in the physics asset */
	int32 ConstraintIndex;

	/** The constrained bone we are parented to in the tree */
	FName BoneName;

	/** The constrained bone we are not parented to in the tree */
	FName OtherBoneName;
};
