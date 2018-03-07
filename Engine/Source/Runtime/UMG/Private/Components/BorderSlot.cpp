// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/BorderSlot.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Components/Border.h"
#include "ObjectEditorUtils.h"

/////////////////////////////////////////////////////
// UBorderSlot

UBorderSlot::UBorderSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
}

void UBorderSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Border.Reset();
}

void UBorderSlot::BuildSlot(TSharedRef<SBorder> InBorder)
{
	Border = InBorder;

	Border->SetPadding(Padding);
	Border->SetHAlign(HorizontalAlignment);
	Border->SetVAlign(VerticalAlignment);

	Border->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UBorderSlot::SetPadding(FMargin InPadding)
{
	CastChecked<UBorder>(Parent)->SetPadding(InPadding);
}

void UBorderSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	CastChecked<UBorder>(Parent)->SetHorizontalAlignment(InHorizontalAlignment);
}

void UBorderSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	CastChecked<UBorder>(Parent)->SetVerticalAlignment(InVerticalAlignment);
}

void UBorderSlot::SynchronizeProperties()
{
	if ( Border.IsValid() )
	{
		SetPadding(Padding);
		SetHorizontalAlignment(HorizontalAlignment);
		SetVerticalAlignment(VerticalAlignment);
	}
}

#if WITH_EDITOR

void UBorderSlot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static bool IsReentrant = false;

	if ( !IsReentrant )
	{
		IsReentrant = true;

		if ( PropertyChangedEvent.Property )
		{
			static const FName PaddingName("Padding");
			static const FName HorizontalAlignmentName("HorizontalAlignment");
			static const FName VerticalAlignmentName("VerticalAlignment");

			FName PropertyName = PropertyChangedEvent.Property->GetFName();

			if ( UBorder* ParentBorder = CastChecked<UBorder>(Parent) )
			{
				if ( PropertyName == PaddingName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, PaddingName, ParentBorder, PaddingName);
				}
				else if ( PropertyName == HorizontalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, HorizontalAlignmentName, ParentBorder, HorizontalAlignmentName);
				}
				else if ( PropertyName == VerticalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, VerticalAlignmentName, ParentBorder, VerticalAlignmentName);
				}
			}
		}

		IsReentrant = false;
	}
}

#endif
