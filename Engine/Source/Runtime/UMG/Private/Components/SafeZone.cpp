// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/SafeZone.h"
#include "SlateFwd.h"

#include "Components/SafeZoneSlot.h"

#define LOCTEXT_NAMESPACE "UMG"

USafeZone::USafeZone()
	: PadLeft(true)
	, PadRight(true)
	, PadTop(true)
	, PadBottom(true)
{
	bCanHaveMultipleChildren = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

#if WITH_EDITOR

const FText USafeZone::GetPaletteCategory()
{
	return LOCTEXT( "Panel", "Panel" );
}

void USafeZone::OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs)
{
	if ( EventArgs.bScreenPreview )
	{
		DesignerSize = EventArgs.Size;
	}
	else
	{
		DesignerSize = FVector2D(0, 0);
	}

	DesignerDpi = EventArgs.DpiScale;

	if ( MySafeZone.IsValid() )
	{
		MySafeZone->SetOverrideScreenInformation(DesignerSize, DesignerDpi);
	}
}

#endif

void USafeZone::OnSlotAdded( UPanelSlot* InSlot )
{
	Super::OnSlotAdded( InSlot );

	UpdateWidgetProperties();
}

void USafeZone::OnSlotRemoved( UPanelSlot* InSlot )
{
	Super::OnSlotRemoved( InSlot );

	if ( MySafeZone.IsValid() )
	{
		MySafeZone->SetContent( SNullWidget::NullWidget );
	}
}

UClass* USafeZone::GetSlotClass() const
{
	return USafeZoneSlot::StaticClass();
}

void USafeZone::UpdateWidgetProperties()
{
	if ( MySafeZone.IsValid() && GetChildrenCount() > 0 )
	{
		USafeZoneSlot* SafeSlot = CastChecked< USafeZoneSlot >( Slots[ 0 ] );

		MySafeZone->SetSafeAreaScale( SafeSlot->SafeAreaScale );
		MySafeZone->SetTitleSafe( SafeSlot->bIsTitleSafe );
		MySafeZone->SetHAlign( SafeSlot->HAlign.GetValue() );
		MySafeZone->SetVAlign( SafeSlot->VAlign.GetValue() );
		MySafeZone->SetPadding( SafeSlot->Padding );
		MySafeZone->SetSidesToPad( PadLeft, PadRight, PadTop, PadBottom );
	}
}

void USafeZone::SetSidesToPad(bool InPadLeft, bool InPadRight, bool InPadTop, bool InPadBottom)
{
	PadLeft = InPadLeft;
	PadRight = InPadRight;
	PadTop = InPadTop;
	PadBottom = InPadBottom;

	if (MySafeZone.IsValid() && GetChildrenCount() > 0)
	{
		MySafeZone->SetSidesToPad(PadLeft, PadRight, PadTop, PadBottom);
	}
}

TSharedRef<SWidget> USafeZone::RebuildWidget()
{
	USafeZoneSlot* SafeSlot = Slots.Num() > 0 ? Cast< USafeZoneSlot >( Slots[ 0 ] ) : nullptr;
	
	MySafeZone = SNew( SSafeZone )
		.IsTitleSafe( SafeSlot ? SafeSlot->bIsTitleSafe : false )
		.SafeAreaScale( SafeSlot ? SafeSlot->SafeAreaScale : FMargin(1,1,1,1))
		.HAlign( SafeSlot ? SafeSlot->HAlign.GetValue() : HAlign_Fill )
		.VAlign( SafeSlot ? SafeSlot->VAlign.GetValue() : VAlign_Fill )
		.Padding( SafeSlot ? SafeSlot->Padding : FMargin() )
		.PadLeft( PadLeft )
		.PadRight( PadRight )
		.PadTop( PadTop )
		.PadBottom( PadBottom )
#if WITH_EDITOR
		.OverrideScreenSize(DesignerSize)
		.OverrideDpiScale(DesignerDpi)
#endif
		[
			GetChildAt( 0 ) ? GetChildAt( 0 )->TakeWidget() : SNullWidget::NullWidget
		];

	return MySafeZone.ToSharedRef();
}

void USafeZone::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySafeZone.Reset();
}

#undef LOCTEXT_NAMESPACE
