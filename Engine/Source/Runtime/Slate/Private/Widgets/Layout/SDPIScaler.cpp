// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SDPIScaler.h"
#include "Layout/ArrangedChildren.h"

SDPIScaler::SDPIScaler()
: ChildSlot()
{
	bCanTick = false;
	bCanSupportFocus = false;
}

void SDPIScaler::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		InArgs._Content.Widget
	];
	
	DPIScale = InArgs._DPIScale;
}

void SDPIScaler::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility MyVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyVisibility ) )
	{
		const float MyDPIScale = DPIScale.Get();

		ArrangedChildren.AddWidget( AllottedGeometry.MakeChild(
			this->ChildSlot.GetWidget(),
			FVector2D::ZeroVector,
			AllottedGeometry.GetLocalSize() / MyDPIScale,
			MyDPIScale
		));

	}
}
	
FVector2D SDPIScaler::ComputeDesiredSize( float ) const
{
	return DPIScale.Get() * ChildSlot.GetWidget()->GetDesiredSize();
}

FChildren* SDPIScaler::GetChildren()
{
	return &ChildSlot;
}

void SDPIScaler::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SDPIScaler::SetDPIScale(TAttribute<float> InDPIScale)
{
	DPIScale = InDPIScale;
}

float SDPIScaler::GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const
{
	return DPIScale.Get();
}
