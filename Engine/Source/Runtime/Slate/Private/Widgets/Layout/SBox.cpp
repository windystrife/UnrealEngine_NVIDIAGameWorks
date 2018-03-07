// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SBox.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/LayoutUtils.h"

SBox::SBox()
: ChildSlot()
{
	bCanTick = false;
	bCanSupportFocus = false;
}

void SBox::Construct( const FArguments& InArgs )
{
	WidthOverride = InArgs._WidthOverride;
	HeightOverride = InArgs._HeightOverride;

	MinDesiredWidth = InArgs._MinDesiredWidth;
	MinDesiredHeight = InArgs._MinDesiredHeight;
	MaxDesiredWidth = InArgs._MaxDesiredWidth;
	MaxDesiredHeight = InArgs._MaxDesiredHeight;

	MaxAspectRatio = InArgs._MaxAspectRatio;

	ChildSlot
		.HAlign( InArgs._HAlign )
		.VAlign( InArgs._VAlign )
		.Padding( InArgs._Padding )
	[
		InArgs._Content.Widget
	];
}

void SBox::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SBox::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SBox::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SBox::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

void SBox::SetWidthOverride(TAttribute<FOptionalSize> InWidthOverride)
{
	if (WidthOverride.Get().Get() != InWidthOverride.Get().Get())
	{
		WidthOverride = InWidthOverride;

		Invalidate(EInvalidateWidget::Layout);
	}
}

void SBox::SetHeightOverride(TAttribute<FOptionalSize> InHeightOverride)
{
	if (HeightOverride.Get().Get() != InHeightOverride.Get().Get())
	{
		HeightOverride = InHeightOverride;

		Invalidate(EInvalidateWidget::Layout);
	}
}

void SBox::SetMinDesiredWidth(TAttribute<FOptionalSize> InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
}

void SBox::SetMinDesiredHeight(TAttribute<FOptionalSize> InMinDesiredHeight)
{
	MinDesiredHeight = InMinDesiredHeight;
}

void SBox::SetMaxDesiredWidth(TAttribute<FOptionalSize> InMaxDesiredWidth)
{
	MaxDesiredWidth = InMaxDesiredWidth;
}

void SBox::SetMaxDesiredHeight(TAttribute<FOptionalSize> InMaxDesiredHeight)
{
	MaxDesiredHeight = InMaxDesiredHeight;
}

void SBox::SetMaxAspectRatio(TAttribute<FOptionalSize> InMaxAspectRatio)
{
	MaxAspectRatio = InMaxAspectRatio;
}

FVector2D SBox::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		// If the user specified a fixed width or height, those values override the Box's content.
		const FVector2D& UnmodifiedChildDesiredSize = ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
		const FOptionalSize CurrentWidthOverride = WidthOverride.Get();
		const FOptionalSize CurrentHeightOverride = HeightOverride.Get();
		const FOptionalSize CurrentMinDesiredWidth = MinDesiredWidth.Get();
		const FOptionalSize CurrentMinDesiredHeight = MinDesiredHeight.Get();
		const FOptionalSize CurrentMaxDesiredWidth = MaxDesiredWidth.Get();
		const FOptionalSize CurrentMaxDesiredHeight = MaxDesiredHeight.Get();

		float CurrentWidth = UnmodifiedChildDesiredSize.X;
		float CurrentHeight = UnmodifiedChildDesiredSize.Y;

		if ( CurrentMinDesiredWidth.IsSet() )
		{
			CurrentWidth = FMath::Max(CurrentWidth, CurrentMinDesiredWidth.Get());
		}

		if ( CurrentMaxDesiredWidth.IsSet() )
		{
			CurrentWidth = FMath::Min(CurrentWidth, CurrentMaxDesiredWidth.Get());
		}

		if ( CurrentMinDesiredHeight.IsSet() )
		{
			CurrentHeight = FMath::Max(CurrentHeight, CurrentMinDesiredHeight.Get());
		}

		if ( CurrentMaxDesiredHeight.IsSet() )
		{
			CurrentHeight = FMath::Min(CurrentHeight, CurrentMaxDesiredHeight.Get());
		}

		return FVector2D(
			( CurrentWidthOverride.IsSet() ) ? CurrentWidthOverride.Get() : CurrentWidth,
			( CurrentHeightOverride.IsSet() ) ? CurrentHeightOverride.Get() : CurrentHeight
		);
	}
	
	return FVector2D::ZeroVector;
}

void SBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility& MyCurrentVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyCurrentVisibility ) )
	{
		const FOptionalSize CurrentMaxAspectRatio = MaxAspectRatio.Get();
		const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
		bool bAlignChildren = true;

		AlignmentArrangeResult XAlignmentResult(0, 0);
		AlignmentArrangeResult YAlignmentResult(0, 0);

		if ( CurrentMaxAspectRatio.IsSet() )
		{
			float CurrentWidth = FMath::Min(AllottedGeometry.Size.X, ChildSlot.GetWidget()->GetDesiredSize().X);
			float CurrentHeight = FMath::Min(AllottedGeometry.Size.Y, ChildSlot.GetWidget()->GetDesiredSize().Y);

			float AspectRatioWidth = CurrentMaxAspectRatio.Get();
			if ( AspectRatioWidth != 0 && CurrentHeight > 0 && CurrentWidth > 0 )
			{
				const float AspectRatioHeight = 1.0f / AspectRatioWidth;

				const float CurrentRatioWidth = ( AllottedGeometry.GetLocalSize().X / AllottedGeometry.GetLocalSize().Y );
				const float CurrentRatioHeight = 1.0f / CurrentRatioWidth;

				if ( CurrentRatioWidth > AspectRatioWidth /*|| CurrentRatioHeight > AspectRatioHeight*/ )
				{
					XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding);
					YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding);

					float NewWidth = AspectRatioWidth * XAlignmentResult.Size;
					float NewHeight = AspectRatioHeight * NewWidth;

					const float MaxWidth = AllottedGeometry.Size.X - SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();
					const float MaxHeight = AllottedGeometry.Size.Y - SlotPadding.GetTotalSpaceAlong<Orient_Vertical>();

					if ( NewWidth > MaxWidth )
					{
						float Scale = MaxWidth / NewWidth;
						NewWidth *= Scale;
						NewHeight *= Scale;
					}

					if ( NewHeight > MaxHeight )
					{
						float Scale = MaxHeight / NewHeight;
						NewWidth *= Scale;
						NewHeight *= Scale;
					}

					XAlignmentResult.Size = NewWidth;
					YAlignmentResult.Size = NewHeight;

					bAlignChildren = false;
				}
			}
		}

		if ( bAlignChildren )
		{
			XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding);
			YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding);
		}

		const float AlignedSizeX = XAlignmentResult.Size;
		const float AlignedSizeY = YAlignmentResult.Size;

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XAlignmentResult.Offset, YAlignmentResult.Offset),
				FVector2D(AlignedSizeX, AlignedSizeY)
			)
		);
	}
}

FChildren* SBox::GetChildren()
{
	return &ChildSlot;
}

int32 SBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// An SBox just draws its only child
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Maybe none of our children are visible
	if( ArrangedChildren.Num() > 0 )
	{
		check( ArrangedChildren.Num() == 1 );
		FArrangedWidget& TheChild = ArrangedChildren[0];

		return TheChild.Widget->Paint( Args.WithNewParent(this), TheChild.Geometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}
	return LayerId;
}
