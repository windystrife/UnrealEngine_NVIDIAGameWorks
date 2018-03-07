// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SClippingHorizontalBox.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"

void SClippingHorizontalBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Call ArrangeChildren so we can cache off the index of the first clipped tab
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);
	ClippedIdx = ArrangedChildren.Num() - 1;
}

void SClippingHorizontalBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	// If WrapButton hasn't been initialized, that means AddWrapButton() hasn't 
	// been called and this method isn't going to behave properly
	check(WrapButton.IsValid());

	SHorizontalBox::OnArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Remove children that are clipped by the allotted geometry
	const int32 NumChildren = ArrangedChildren.Num(); 
	int32 IndexClippedAt = NumChildren;
	for (int32 ChildIdx = NumChildren - 2; ChildIdx >= 0; --ChildIdx)
	{
		const FArrangedWidget& CurWidget = ArrangedChildren[ChildIdx];
		if (FMath::TruncToInt(CurWidget.Geometry.AbsolutePosition.X + CurWidget.Geometry.GetLocalSize().X * CurWidget.Geometry.Scale) > FMath::TruncToInt(AllottedGeometry.AbsolutePosition.X + AllottedGeometry.GetLocalSize().X * CurWidget.Geometry.Scale))
		{
			ArrangedChildren.Remove(ChildIdx);
			IndexClippedAt = ChildIdx;
		}
	}

	if (IndexClippedAt == NumChildren)
	{
		// None of the children are being clipped, so remove the wrap button
		ArrangedChildren.Remove(ArrangedChildren.Num() - 1);
	}
	else
	{
		// Right align the wrap button
		FArrangedWidget& ArrangedButton = ArrangedChildren[ArrangedChildren.Num() - 1];
		ArrangedButton.Geometry = AllottedGeometry.MakeChild(ArrangedButton.Geometry.GetLocalSize(), FSlateLayoutTransform(AllottedGeometry.GetLocalSize() - ArrangedButton.Geometry.GetLocalSize()));
		const int32 WrapButtonXPosition = FMath::TruncToInt(ArrangedButton.Geometry.AbsolutePosition.X);

		// Further remove any children that the wrap button overlaps with
		for (int32 ChildIdx = IndexClippedAt - 1; ChildIdx >= 0; --ChildIdx)
		{
			const FArrangedWidget& CurWidget = ArrangedChildren[ChildIdx];
			if (FMath::TruncToInt(CurWidget.Geometry.AbsolutePosition.X + CurWidget.Geometry.GetLocalSize().X * CurWidget.Geometry.Scale) > WrapButtonXPosition)
			{
				ArrangedChildren.Remove(ChildIdx);
			}
		}
	}
}

int32 SClippingHorizontalBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Get the clipped children info
	FArrangedChildren ClippedArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ClippedArrangedChildren);
	
	// Get the non-clipped children info
	// @todo umg: One should not call the virtual OnArrangeChildren, one should only call ArrangeChildren.
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	SBoxPanel::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
	
	if ((ClippedArrangedChildren.Num() != 0) && (ArrangedChildren.Num() != 0))
	{
		int32 IndexClippedAt = ClippedArrangedChildren.Num() - 1;
		const FArrangedWidget& LastCippedChild = ClippedArrangedChildren[IndexClippedAt];
		const FArrangedWidget& FirstChild = ArrangedChildren[0];
		const FArrangedWidget& LastChild = ArrangedChildren[ArrangedChildren.Num() - 1];
		float BorderLocalWidth = AllottedGeometry.GetLocalSize().X;
	
		// If only the last child/block, which is the wrap button, is being clipped
		if (IndexClippedAt == ArrangedChildren.Num() - 2)
		{
			// Only recalculate the alloted geometry size if said size is fitted to the toolbar/menubar
			if (FMath::TruncToInt(AllottedGeometry.AbsolutePosition.X + AllottedGeometry.GetLocalSize().X * AllottedGeometry.Scale) <= FMath::TruncToInt(LastChild.Geometry.AbsolutePosition.X + LastChild.Geometry.GetLocalSize().X * LastChild.Geometry.Scale))
			{
				// Calculate the size of the custom border
				BorderLocalWidth = (LastCippedChild.Geometry.AbsolutePosition.X + LastCippedChild.Geometry.GetLocalSize().X * LastCippedChild.Geometry.Scale - FirstChild.Geometry.AbsolutePosition.X) / AllottedGeometry.Scale;
			}
		}
		else
		{
			// Children/blocks are being clipped, calculate the size of the custom border
			const FArrangedWidget& NextChild = (IndexClippedAt + 1 < ClippedArrangedChildren.Num())? ClippedArrangedChildren[IndexClippedAt + 1]: LastCippedChild;
			BorderLocalWidth = (NextChild.Geometry.AbsolutePosition.X + NextChild.Geometry.GetLocalSize().X * NextChild.Geometry.Scale - FirstChild.Geometry.AbsolutePosition.X) / AllottedGeometry.Scale;
		}
	
		bool bEnabled = ShouldBeEnabled( bParentEnabled );
		ESlateDrawEffect DrawEffects = !bEnabled ? ESlateDrawEffect::DisabledEffect : ESlateDrawEffect::None;
		FSlateColor BorderBackgroundColor = FLinearColor::White;

		// Draw the custom border
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(BorderLocalWidth, AllottedGeometry.GetLocalSize().Y), FSlateLayoutTransform()),
			BackgroundBrush,
			DrawEffects,
			BackgroundBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint() * BorderBackgroundColor.GetColor(InWidgetStyle)
			);
	}

	return SHorizontalBox::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FVector2D SClippingHorizontalBox::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D Size = SBoxPanel::ComputeDesiredSize(LayoutScaleMultiplier);
	if (ClippedIdx == (Children.Num() - 2))
	{
		// If the wrap button isn't being shown, subtract it's size from the total desired size
		const SBoxPanel::FSlot& Child = Children[Children.Num() - 1];
		const FVector2D& ChildDesiredSize = Child.GetWidget()->GetDesiredSize();
		Size.X -= ChildDesiredSize.X;
	}
	return Size;
}

void SClippingHorizontalBox::Construct( const FArguments& InArgs )
{
	BackgroundBrush = InArgs._BackgroundBrush;
	OnWrapButtonClicked = InArgs._OnWrapButtonClicked;
	StyleSet = InArgs._StyleSet;
	StyleName = InArgs._StyleName;
}

void SClippingHorizontalBox::AddWrapButton()
{
	// Construct the wrap button used in toolbars and menubars
	WrapButton = 
		SNew( SComboButton )
		.HasDownArrow( false )
		.ButtonStyle( StyleSet, ISlateStyle::Join(StyleName, ".Button") )
		.ContentPadding( 0 )
		.ToolTipText( NSLOCTEXT("Slate", "ExpandToolbar", "Click to expand toolbar") )
		.OnGetMenuContent( OnWrapButtonClicked )
		.Cursor( EMouseCursor::Default )
		.ButtonContent()
		[
			SNew( SImage )
			.Image( StyleSet->GetBrush(StyleName, ".Expand") )
		];

	// Add the wrap button
	AddSlot()
	.AutoWidth()
	.Padding( 0 )
	[
		WrapButton.ToSharedRef()
	];
}

