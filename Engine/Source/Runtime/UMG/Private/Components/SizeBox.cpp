// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/SizeBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Components/SizeBoxSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USizeBox

USizeBox::USizeBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
	MaxAspectRatio = 1;
}

void USizeBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySizeBox.Reset();
}

TSharedRef<SWidget> USizeBox::RebuildWidget()
{
	MySizeBox = SNew(SBox);
	
	if ( GetChildrenCount() > 0 )
	{
		Cast<USizeBoxSlot>(GetContentSlot())->BuildSlot(MySizeBox.ToSharedRef());
	}

	return MySizeBox.ToSharedRef();
}

void USizeBox::SetWidthOverride(float InWidthOverride)
{
	bOverride_WidthOverride = true;
	WidthOverride = InWidthOverride;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetWidthOverride(InWidthOverride);
	}
}

void USizeBox::ClearWidthOverride()
{
	bOverride_WidthOverride = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetWidthOverride(FOptionalSize());
	}
}

void USizeBox::SetHeightOverride(float InHeightOverride)
{
	bOverride_HeightOverride = true;
	HeightOverride = InHeightOverride;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetHeightOverride(InHeightOverride);
	}
}

void USizeBox::ClearHeightOverride()
{
	bOverride_HeightOverride = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetHeightOverride(FOptionalSize());
	}
}

void USizeBox::SetMinDesiredWidth(float InMinDesiredWidth)
{
	bOverride_MinDesiredWidth = true;
	MinDesiredWidth = InMinDesiredWidth;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredWidth(InMinDesiredWidth);
	}
}

void USizeBox::ClearMinDesiredWidth()
{
	bOverride_MinDesiredWidth = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredWidth(FOptionalSize());
	}
}

void USizeBox::SetMinDesiredHeight(float InMinDesiredHeight)
{
	bOverride_MinDesiredHeight = true;
	MinDesiredHeight = InMinDesiredHeight;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredHeight(InMinDesiredHeight);
	}
}

void USizeBox::ClearMinDesiredHeight()
{
	bOverride_MinDesiredHeight = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredHeight(FOptionalSize());
	}
}

void USizeBox::SetMaxDesiredWidth(float InMaxDesiredWidth)
{
	bOverride_MaxDesiredWidth = true;
	MaxDesiredWidth = InMaxDesiredWidth;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredWidth(InMaxDesiredWidth);
	}
}

void USizeBox::ClearMaxDesiredWidth()
{
	bOverride_MaxDesiredWidth = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredWidth(FOptionalSize());
	}
}

void USizeBox::SetMaxDesiredHeight(float InMaxDesiredHeight)
{
	bOverride_MaxDesiredHeight = true;
	MaxDesiredHeight = InMaxDesiredHeight;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredHeight(InMaxDesiredHeight);
	}
}

void USizeBox::ClearMaxDesiredHeight()
{
	bOverride_MaxDesiredHeight = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredHeight(FOptionalSize());
	}
}

void USizeBox::SetMaxAspectRatio(float InMaxAspectRatio)
{
	bOverride_MaxAspectRatio = true;
	MaxAspectRatio = InMaxAspectRatio;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxAspectRatio(InMaxAspectRatio);
	}
}

void USizeBox::ClearMaxAspectRatio()
{
	bOverride_MaxAspectRatio = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxAspectRatio(FOptionalSize());
	}
}

void USizeBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( bOverride_WidthOverride )
	{
		SetWidthOverride(WidthOverride);
	}
	else
	{
		ClearWidthOverride();
	}

	if ( bOverride_HeightOverride )
	{
		SetHeightOverride(HeightOverride);
	}
	else
	{
		ClearHeightOverride();
	}

	if ( bOverride_MinDesiredWidth )
	{
		SetMinDesiredWidth(MinDesiredWidth);
	}
	else
	{
		ClearMinDesiredWidth();
	}

	if ( bOverride_MinDesiredHeight )
	{
		SetMinDesiredHeight(MinDesiredHeight);
	}
	else
	{
		ClearMinDesiredHeight();
	}

	if ( bOverride_MaxDesiredWidth )
	{
		SetMaxDesiredWidth(MaxDesiredWidth);
	}
	else
	{
		ClearMaxDesiredWidth();
	}

	if ( bOverride_MaxDesiredHeight )
	{
		SetMaxDesiredHeight(MaxDesiredHeight);
	}
	else
	{
		ClearMaxDesiredHeight();
	}

	if ( bOverride_MaxAspectRatio )
	{
		SetMaxAspectRatio(MaxAspectRatio);
	}
	else
	{
		ClearMaxAspectRatio();
	}
}

UClass* USizeBox::GetSlotClass() const
{
	return USizeBoxSlot::StaticClass();
}

void USizeBox::OnSlotAdded(UPanelSlot* InSlot)
{
	// Add the child to the live slot if it already exists
	if ( MySizeBox.IsValid() )
	{
		CastChecked<USizeBoxSlot>(InSlot)->BuildSlot(MySizeBox.ToSharedRef());
	}
}

void USizeBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	// Remove the widget from the live slot if it exists.
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetContent(SNullWidget::NullWidget);
	}
}

#if WITH_EDITOR

const FText USizeBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
