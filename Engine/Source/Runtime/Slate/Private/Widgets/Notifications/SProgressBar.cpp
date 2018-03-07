// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Notifications/SProgressBar.h"
#include "Rendering/DrawElements.h"


void SProgressBar::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	MarqueeOffset = 0.0f;

	Style = InArgs._Style;

	SetPercent(InArgs._Percent);
	BarFillType = InArgs._BarFillType;
	
	BackgroundImage = InArgs._BackgroundImage;
	FillImage = InArgs._FillImage;
	MarqueeImage = InArgs._MarqueeImage;
	
	FillColorAndOpacity = InArgs._FillColorAndOpacity;
	BorderPadding = InArgs._BorderPadding;

	CurrentTickRate = 0.0f;
	MinimumTickRate = InArgs._RefreshRate;

	ActiveTimerHandle = RegisterActiveTimer(CurrentTickRate, FWidgetActiveTimerDelegate::CreateSP(this, &SProgressBar::ActiveTick));
}

void SProgressBar::SetPercent(TAttribute< TOptional<float> > InPercent)
{
	if ( !Percent.IdenticalTo(InPercent) )
	{
		Percent = InPercent;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SProgressBar::SetStyle(const FProgressBarStyle* InStyle)
{
	Style = InStyle;

	if (Style == nullptr)
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetBarFillType(EProgressBarFillType::Type InBarFillType)
{
	BarFillType = InBarFillType;
	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetFillColorAndOpacity(TAttribute< FSlateColor > InFillColorAndOpacity)
{
	FillColorAndOpacity = InFillColorAndOpacity;
	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetBorderPadding(TAttribute< FVector2D > InBorderPadding)
{
	BorderPadding = InBorderPadding;
	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetBackgroundImage(const FSlateBrush* InBackgroundImage)
{
	BackgroundImage = InBackgroundImage;
	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetFillImage(const FSlateBrush* InFillImage)
{
	FillImage = InFillImage;
	Invalidate(EInvalidateWidget::Layout);
}

void SProgressBar::SetMarqueeImage(const FSlateBrush* InMarqueeImage)
{
	MarqueeImage = InMarqueeImage;
	Invalidate(EInvalidateWidget::Layout);
}

const FSlateBrush* SProgressBar::GetBackgroundImage() const
{
	return BackgroundImage ? BackgroundImage : &Style->BackgroundImage;
}

const FSlateBrush* SProgressBar::GetFillImage() const
{
	return FillImage ? FillImage : &Style->FillImage;
}

const FSlateBrush* SProgressBar::GetMarqueeImage() const
{
	return MarqueeImage ? MarqueeImage : &Style->MarqueeImage;
}

void PushTransformedClip(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, FVector2D InsetPadding, FVector2D ProgressOrigin, FSlateRect Progress)
{
	const FSlateRenderTransform& Transform = AllottedGeometry.GetAccumulatedRenderTransform();

	const FVector2D MaxSize = AllottedGeometry.GetLocalSize() - ( InsetPadding * 2.0f );

	OutDrawElements.PushClip(FSlateClippingZone(
		Transform.TransformPoint(InsetPadding + (ProgressOrigin - FVector2D(Progress.Left, Progress.Top)) * MaxSize),
		Transform.TransformPoint(InsetPadding + FVector2D(ProgressOrigin.X + Progress.Right, ProgressOrigin.Y - Progress.Top) * MaxSize),
		Transform.TransformPoint(InsetPadding + FVector2D(ProgressOrigin.X - Progress.Left, ProgressOrigin.Y + Progress.Bottom) * MaxSize),
		Transform.TransformPoint(InsetPadding + (ProgressOrigin + FVector2D(Progress.Right, Progress.Bottom)) * MaxSize)
	));
}

int32 SProgressBar::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// Used to track the layer ID we will return.
	int32 RetLayerId = LayerId;

	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const FSlateBrush* CurrentFillImage = GetFillImage();
	
	const FLinearColor FillColorAndOpacitySRGB(InWidgetStyle.GetColorAndOpacityTint() * FillColorAndOpacity.Get().GetColor(InWidgetStyle) * CurrentFillImage->GetTint(InWidgetStyle));
	const FLinearColor ColorAndOpacitySRGB = InWidgetStyle.GetColorAndOpacityTint();

	TOptional<float> ProgressFraction = Percent.Get();	
	FVector2D BorderPaddingRef = BorderPadding.Get();

	const FSlateBrush* CurrentBackgroundImage = GetBackgroundImage();

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		CurrentBackgroundImage,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * CurrentBackgroundImage->GetTint( InWidgetStyle )
	);	
	
	if( ProgressFraction.IsSet() )
	{
		const float ClampedFraction = FMath::Clamp(ProgressFraction.GetValue(), 0.0f, 1.0f);

		switch (BarFillType)
		{
			case EProgressBarFillType::RightToLeft:
			{
				PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(1, 0), FSlateRect(ClampedFraction, 0, 0, 1));

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y )),
					CurrentFillImage,
					DrawEffects,
					FillColorAndOpacitySRGB
					);

				OutDrawElements.PopClip();
				break;
			}
			case EProgressBarFillType::FillFromCenter:
			{
				float HalfFrac = ClampedFraction / 2.0f;
				PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0.5, 0.5), FSlateRect(HalfFrac, HalfFrac, HalfFrac, HalfFrac));

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D( (AllottedGeometry.GetLocalSize().X * 0.5f) - ((AllottedGeometry.GetLocalSize().X * ( ClampedFraction ))*0.5), 0.0f),
						FVector2D( AllottedGeometry.GetLocalSize().X * ( ClampedFraction ) , AllottedGeometry.GetLocalSize().Y )),
					CurrentFillImage,
					DrawEffects,
					FillColorAndOpacitySRGB
					);

				OutDrawElements.PopClip();
				break;
			}
			case EProgressBarFillType::TopToBottom:
			{
				PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0, 0), FSlateRect(0, 0, 1, ClampedFraction));

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y )),
					CurrentFillImage,
					DrawEffects,
					FillColorAndOpacitySRGB
					);

				OutDrawElements.PopClip();
				break;
			}
			case EProgressBarFillType::BottomToTop:
			{
				PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0, 1), FSlateRect(0, ClampedFraction, 1, 0));

				FSlateRect ClippedAllotedGeometry = FSlateRect(AllottedGeometry.AbsolutePosition, AllottedGeometry.AbsolutePosition + AllottedGeometry.GetLocalSize() * AllottedGeometry.Scale);
				ClippedAllotedGeometry.Top = ClippedAllotedGeometry.Bottom - ClippedAllotedGeometry.GetSize().Y * ClampedFraction;

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y )),
					CurrentFillImage,
					DrawEffects,
					FillColorAndOpacitySRGB
					);

				OutDrawElements.PopClip();
				break;
			}
			case EProgressBarFillType::LeftToRight:
			default:
			{
				PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0, 0), FSlateRect(0, 0, ClampedFraction, 1));

				// Draw Fill
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
						FVector2D::ZeroVector,
						FVector2D( AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y )),
					CurrentFillImage,
					DrawEffects,
					FillColorAndOpacitySRGB
					);

				OutDrawElements.PopClip();
				break;
			}
		}
	}
	else
	{
		const FSlateBrush* CurrentMarqueeImage = GetMarqueeImage();
		
		// Draw Marquee
		const float MarqueeAnimOffset = CurrentMarqueeImage->ImageSize.X * MarqueeOffset;
		const float MarqueeImageSize = CurrentMarqueeImage->ImageSize.X;

		PushTransformedClip(OutDrawElements, AllottedGeometry, BorderPaddingRef, FVector2D(0, 0), FSlateRect(0, 0, 1, 1));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			RetLayerId++,
			AllottedGeometry.ToPaintGeometry(
				FVector2D( MarqueeAnimOffset - MarqueeImageSize, 0.0f ),
				FVector2D( AllottedGeometry.GetLocalSize().X + MarqueeImageSize, AllottedGeometry.GetLocalSize().Y )),
			CurrentMarqueeImage,
			DrawEffects,
			CurrentMarqueeImage->TintColor.GetSpecifiedColor() * ColorAndOpacitySRGB
			);

		OutDrawElements.PopClip();
	}

	return RetLayerId - 1;
}

FVector2D SProgressBar::ComputeDesiredSize( float ) const
{
	return GetMarqueeImage()->ImageSize;
}

bool SProgressBar::ComputeVolatility() const
{
	return SLeafWidget::ComputeVolatility() || Percent.IsBound();
}

void SProgressBar::SetActiveTimerTickRate(float TickRate)
{
	if (CurrentTickRate != TickRate || !ActiveTimerHandle.IsValid())
	{
		CurrentTickRate = TickRate;

		TSharedPtr<FActiveTimerHandle> SharedActiveTimerHandle = ActiveTimerHandle.Pin();
		if (SharedActiveTimerHandle.IsValid())
		{
			UnRegisterActiveTimer(SharedActiveTimerHandle.ToSharedRef());
		}

		ActiveTimerHandle = RegisterActiveTimer(TickRate, FWidgetActiveTimerDelegate::CreateSP(this, &SProgressBar::ActiveTick));
	}
}

EActiveTimerReturnType SProgressBar::ActiveTick(double InCurrentTime, float InDeltaTime)
{
	MarqueeOffset = InCurrentTime - FMath::FloorToDouble(InCurrentTime);
	
	TOptional<float> PrecentFracton = Percent.Get();
	if (PrecentFracton.IsSet())
	{
		SetActiveTimerTickRate(MinimumTickRate);
	}
	else
	{
		SetActiveTimerTickRate(0.0f);
	}

	return EActiveTimerReturnType::Continue;
}


