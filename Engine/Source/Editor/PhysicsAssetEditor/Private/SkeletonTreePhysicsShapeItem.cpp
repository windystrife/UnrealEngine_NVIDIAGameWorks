// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreePhysicsShapeItem.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FSkeletonTreePhysicsShapeItem"

FSkeletonTreePhysicsShapeItem::FSkeletonTreePhysicsShapeItem(USkeletalBodySetup* InBodySetup, const FName& InBoneName, int32 InBodySetupIndex, EAggCollisionShape::Type InShapeType, int32 InShapeIndex, const TSharedRef<class ISkeletonTree>& InSkeletonTree)
	: FSkeletonTreeItem(InSkeletonTree)
	, BodySetup(InBodySetup)
	, BodySetupIndex(InBodySetupIndex)
	, ShapeType(InShapeType)
	, ShapeIndex(InShapeIndex)
{
	switch (ShapeType)
	{
	case EAggCollisionShape::Sphere:
		ShapeBrush = FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.Sphere");
		Label = *FText::Format(LOCTEXT("SphereLabel", "{0} Sphere {1}"), FText::FromName(InBoneName), FText::AsNumber(ShapeIndex)).ToString();
		break;
	case EAggCollisionShape::Box:
		ShapeBrush = FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.Box");
		Label = *FText::Format(LOCTEXT("BoxLabel", "{0} Box {1}"), FText::FromName(InBoneName), FText::AsNumber(ShapeIndex)).ToString();
		break;
	case EAggCollisionShape::Sphyl:
		ShapeBrush = FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.Sphyl");
		Label = *FText::Format(LOCTEXT("CapsuleLabel", "{0} Capsule {1}"), FText::FromName(InBoneName), FText::AsNumber(ShapeIndex)).ToString();
		break;
	case EAggCollisionShape::Convex:
		ShapeBrush = FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.Convex");
		Label = *FText::Format(LOCTEXT("ConvexLabel", "{0} Convex {1}"), FText::FromName(InBoneName), FText::AsNumber(ShapeIndex)).ToString();
		break;
	default:
		check(false);
		break;
	}

	RowItemName = *Label.ToString();
}

void FSkeletonTreePhysicsShapeItem::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected )
{
	Box->AddSlot()
	.AutoWidth()
	.Padding(FMargin(0.0f, 1.0f))
	[
		SNew( SImage )
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(ShapeBrush)
	];

	Box->AddSlot()
	.AutoWidth()
	.Padding(2, 0, 0, 0)
	[
		SNew(STextBlock)
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Text(FText::FromName(Label))
		.HighlightText(FilterText)
		.Font(FEditorStyle::GetFontStyle("PhysicsAssetEditor.Tree.Font"))
		.ToolTipText(FText::FromName(Label))
	];
}

TSharedRef< SWidget > FSkeletonTreePhysicsShapeItem::GenerateWidgetForDataColumn(const FName& DataColumnName)
{
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
