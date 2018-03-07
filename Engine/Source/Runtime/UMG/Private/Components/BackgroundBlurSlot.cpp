// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BackgroundBlurSlot.h"
#include "ObjectEditorUtils.h"
#include "SBackgroundBlur.h"
#include "BackgroundBlur.h"

/////////////////////////////////////////////////////
// UBackgroundBlurSlot

UBackgroundBlurSlot::UBackgroundBlurSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Fill;
	VerticalAlignment = VAlign_Fill;
}

void UBackgroundBlurSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	BackgroundBlur.Reset();
}

void UBackgroundBlurSlot::BuildSlot(TSharedRef<SBackgroundBlur> InBackgroundBlur)
{
	BackgroundBlur = InBackgroundBlur;

	BackgroundBlur->SetPadding(Padding);
	BackgroundBlur->SetHAlign(HorizontalAlignment);
	BackgroundBlur->SetVAlign(VerticalAlignment);

	BackgroundBlur->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UBackgroundBlurSlot::SetPadding(FMargin InPadding)
{
	CastChecked<UBackgroundBlur>(Parent)->SetPadding(InPadding);
}

void UBackgroundBlurSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	CastChecked<UBackgroundBlur>(Parent)->SetHorizontalAlignment(InHorizontalAlignment);
}

void UBackgroundBlurSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	CastChecked<UBackgroundBlur>(Parent)->SetVerticalAlignment(InVerticalAlignment);
}

void UBackgroundBlurSlot::SynchronizeProperties()
{
	if ( BackgroundBlur.IsValid() )
	{
		SetPadding(Padding);
		SetHorizontalAlignment(HorizontalAlignment);
		SetVerticalAlignment(VerticalAlignment);
	}
}

#if WITH_EDITOR

void UBackgroundBlurSlot::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
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

			if ( UBackgroundBlur* ParentBackgroundBlur = CastChecked<UBackgroundBlur>(Parent) )
			{
				if (PropertyName == PaddingName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, PaddingName, ParentBackgroundBlur, PaddingName);
				}
				else if (PropertyName == HorizontalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, HorizontalAlignmentName, ParentBackgroundBlur, HorizontalAlignmentName);
				}
				else if (PropertyName == VerticalAlignmentName)
				{
					FObjectEditorUtils::MigratePropertyValue(this, VerticalAlignmentName, ParentBackgroundBlur, VerticalAlignmentName);
				}
			}
		}

		IsReentrant = false;
	}
}

#endif
