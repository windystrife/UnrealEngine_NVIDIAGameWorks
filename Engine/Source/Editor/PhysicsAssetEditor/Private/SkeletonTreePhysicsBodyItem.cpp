// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreePhysicsBodyItem.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FSkeletonTreePhysicsBodyItem"

void FSkeletonTreePhysicsBodyItem::GenerateWidgetForNameColumn( TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected )
{
	Box->AddSlot()
	.AutoWidth()
	.Padding(FMargin(0.0f, 1.0f))
	[
		SNew( SImage )
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(this, &FSkeletonTreePhysicsBodyItem::GetBrush)
	];

	Box->AddSlot()
	.AutoWidth()
	.Padding(2, 0, 0, 0)
	[
		SNew(STextBlock)
		.ColorAndOpacity(this, &FSkeletonTreePhysicsBodyItem::GetBodyTextColor)
		.Text(FText::FromName(BoneName))
		.HighlightText(FilterText)
		.Font(FEditorStyle::GetFontStyle("PhysicsAssetEditor.Tree.Font"))
		.ToolTipText(FText::Format(LOCTEXT("BodyTooltip", "Aggregate physics body for bone '{0}'. Bodies can consist of multiple shapes."), FText::FromName(BoneName)))
	];
}

TSharedRef< SWidget > FSkeletonTreePhysicsBodyItem::GenerateWidgetForDataColumn(const FName& DataColumnName)
{
	return SNullWidget::NullWidget;
}

const FSlateBrush* FSkeletonTreePhysicsBodyItem::GetBrush() const
{
	return BodySetup->PhysicsType == EPhysicsType::PhysType_Kinematic ? FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.KinematicBody") : FEditorStyle::GetBrush("PhysicsAssetEditor.Tree.Body");
}

FSlateColor FSkeletonTreePhysicsBodyItem::GetBodyTextColor() const
{
	FLinearColor Color(1.0f, 1.0f, 1.0f);

	const bool bInCurrentProfile = BodySetup->GetCurrentPhysicalAnimationProfileName() == NAME_None || BodySetup->FindPhysicalAnimationProfile(BodySetup->GetCurrentPhysicalAnimationProfileName()) != nullptr;

	if (FilterResult == ESkeletonTreeFilterResult::ShownDescendant)
	{
		Color = FLinearColor::Gray * 0.5f;
	}

	if(bInCurrentProfile)
	{
		return FSlateColor(Color);
	}
	else
	{
		return FSlateColor(Color.Desaturate(0.5f));
	}
}

#undef LOCTEXT_NAMESPACE
