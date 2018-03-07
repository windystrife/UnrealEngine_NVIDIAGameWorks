// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SScaleBox.h"
#include "Layout/LayoutUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Misc/CoreDelegates.h"


/* SScaleBox interface
 *****************************************************************************/

void SScaleBox::Construct( const SScaleBox::FArguments& InArgs )
{
	Stretch = InArgs._Stretch;
	RefreshSafeZoneScale();

	StretchDirection = InArgs._StretchDirection;
	UserSpecifiedScale = InArgs._UserSpecifiedScale;
	IgnoreInheritedScale = InArgs._IgnoreInheritedScale;
	bSingleLayoutPass = InArgs._SingleLayoutPass;

	LastIncomingScale = 1.0f;
	LastAreaSize = FVector2D(0, 0);
	LastFinalOffset = FVector2D(0, 0);

	ChildSlot
	.HAlign(InArgs._HAlign)
	.VAlign(InArgs._VAlign)
	[
		InArgs._Content.Widget
	];

	OnSafeFrameChangedHandle = FCoreDelegates::OnSafeFrameChangedEvent.AddSP(this, &SScaleBox::RefreshSafeZoneScale);
}

SScaleBox::~SScaleBox()
{
	FCoreDelegates::OnSafeFrameChangedEvent.Remove(OnSafeFrameChangedHandle);
}

/* SWidget overrides
 *****************************************************************************/

void SScaleBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FVector2D AreaSize = AllottedGeometry.GetLocalSize();
		FVector2D SlotWidgetDesiredSize = ChildSlot.GetWidget()->GetDesiredSize();

		float FinalScale = 1;

		bool bAllowFullLayout = true;

		if ( bSingleLayoutPass && LastContentDesiredSize.IsSet() && LastFinalScale.IsSet() && LastAreaSize.Equals(AreaSize) && FMath::IsNearlyEqual(LastIncomingScale, AllottedGeometry.Scale) )
		{
			if ( SlotWidgetDesiredSize.Equals(LastContentDesiredSize.GetValue()) )
			{
				bAllowFullLayout = false;
				FinalScale = LastFinalScale.GetValue();
			}
		}

		if ( bAllowFullLayout )
		{
			const EStretch::Type CurrentStretch = Stretch.Get();
			const EStretchDirection::Type CurrentStretchDirection = StretchDirection.Get();

			bool bRequiresAnotherPrepass = CurrentStretch != EStretch::UserSpecified && CurrentStretch != EStretch::ScaleBySafeZone;

			if ( SlotWidgetDesiredSize.X != 0 && SlotWidgetDesiredSize.Y != 0 )
			{
				switch ( CurrentStretch )
				{
				case EStretch::None:
					bRequiresAnotherPrepass = false;
					break;
				case EStretch::Fill:
					SlotWidgetDesiredSize = AreaSize;
					bRequiresAnotherPrepass = false;
					break;
				case EStretch::ScaleToFit:
					FinalScale = FMath::Min(AreaSize.X / SlotWidgetDesiredSize.X, AreaSize.Y / SlotWidgetDesiredSize.Y);
					break;
				case EStretch::ScaleToFitX:
					FinalScale = AreaSize.X / SlotWidgetDesiredSize.X;
					break;
				case EStretch::ScaleToFitY:
					FinalScale = AreaSize.Y / SlotWidgetDesiredSize.Y;
					break;
				case EStretch::ScaleToFill:
					FinalScale = FMath::Max(AreaSize.X / SlotWidgetDesiredSize.X, AreaSize.Y / SlotWidgetDesiredSize.Y);
					break;
				case EStretch::ScaleBySafeZone:
					FinalScale = SafeZoneScale;
					bRequiresAnotherPrepass = false;
					break;
				case EStretch::UserSpecified:
					FinalScale = UserSpecifiedScale.Get(1.0f);
					bRequiresAnotherPrepass = false;
					break;
				}

				switch ( CurrentStretchDirection )
				{
				case EStretchDirection::DownOnly:
					FinalScale = FMath::Min(FinalScale, 1.0f);
					break;
				case EStretchDirection::UpOnly:
					FinalScale = FMath::Max(FinalScale, 1.0f);
					break;
				case EStretchDirection::Both:
					break;
				}

				LastFinalScale = FinalScale;
			}
			else
			{
				LastFinalScale.Reset();
			}

			if ( IgnoreInheritedScale.Get(false) && AllottedGeometry.Scale != 0 )
			{
				FinalScale /= AllottedGeometry.Scale;
			}

			LastFinalOffset = FVector2D(0, 0);

			// If we're just filling, there's no scale applied, we're just filling the area.
			if ( CurrentStretch != EStretch::Fill )
			{
				const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
				AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AreaSize.X, ChildSlot, SlotPadding, FinalScale, false);
				AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AreaSize.Y, ChildSlot, SlotPadding, FinalScale, false);

				LastFinalOffset = FVector2D(XResult.Offset, YResult.Offset) / FinalScale;

				// If the layout horizontally is fill, then we need the desired size to be the whole size of the widget, 
				// but scale the inverse of the scale we're applying.
				if ( ChildSlot.HAlignment == HAlign_Fill )
				{
					SlotWidgetDesiredSize.X = AreaSize.X / FinalScale;
				}

				// If the layout vertically is fill, then we need the desired size to be the whole size of the widget, 
				// but scale the inverse of the scale we're applying.
				if ( ChildSlot.VAlignment == VAlign_Fill )
				{
					SlotWidgetDesiredSize.Y = AreaSize.Y / FinalScale;
				}
			}

			LastAreaSize = AreaSize;
			LastIncomingScale = AllottedGeometry.Scale;
			LastSlotWidgetDesiredSize = SlotWidgetDesiredSize;

			if ( bRequiresAnotherPrepass )
			{
				// We need to run another pre-pass now that we know the final scale.
				// This will allow things that don't scale linearly (such as text) to update their size and layout correctly.
				//
				// NOTE: This step is pretty expensive especially if you're nesting scale boxes.
				ChildSlot.GetWidget()->SlatePrepass(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale() * FinalScale);

				LastContentDesiredSize = ChildSlot.GetWidget()->GetDesiredSize();
			}
			else
			{
				LastContentDesiredSize.Reset();
				LastFinalScale.Reset();
			}
		}

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			LastFinalOffset,
			LastSlotWidgetDesiredSize,
			FinalScale
		) );
	}
}

int32 SScaleBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	bool bClippingNeeded = false;

	if (GetClipping() == EWidgetClipping::Inherit)
	{
		const EStretch::Type CurrentStretch = Stretch.Get();

		// There are a few stretch modes that require we clip, even if the user didn't set the property.
		switch (CurrentStretch)
		{
		case EStretch::ScaleToFitX:
		case EStretch::ScaleToFitY:
		case EStretch::ScaleToFill:
			bClippingNeeded = true;
			break;
		}
	}

	if (bClippingNeeded)
	{
		OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));
		FGeometry HitTestGeometry = AllottedGeometry;
		HitTestGeometry.AppendTransform(FSlateLayoutTransform(Args.GetWindowToDesktopTransform()));
		Args.GetGrid().PushClip(FSlateClippingZone(HitTestGeometry));
	}

	int32 MaxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	if (bClippingNeeded)
	{
		OutDrawElements.PopClip();
		Args.GetGrid().PopClip();
	}

	return MaxLayerId;
}

void SScaleBox::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SScaleBox::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SScaleBox::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SScaleBox::SetStretchDirection(EStretchDirection::Type InStretchDirection)
{
	StretchDirection = InStretchDirection;
}

void SScaleBox::SetStretch(EStretch::Type InStretch)
{
	Stretch = InStretch;
	RefreshSafeZoneScale();
}

void SScaleBox::SetUserSpecifiedScale(float InUserSpecifiedScale)
{
	UserSpecifiedScale = InUserSpecifiedScale;
}

void SScaleBox::SetIgnoreInheritedScale(bool InIgnoreInheritedScale)
{
	IgnoreInheritedScale = InIgnoreInheritedScale;
}

FVector2D SScaleBox::ComputeDesiredSize(float InScale) const
{
	float LayoutScale = GetLayoutScale();
	if (IgnoreInheritedScale.Get(false))
	{
		return LayoutScale * SCompoundWidget::ComputeDesiredSize(InScale) / InScale;
	}
	return LayoutScale * SCompoundWidget::ComputeDesiredSize(InScale);
}

float SScaleBox::GetRelativeLayoutScale(const FSlotBase& Child, float LayoutScaleMultiplier) const
{
	if ( IgnoreInheritedScale.Get(false) )
	{
		return GetLayoutScale() / LayoutScaleMultiplier;
	}

	return GetLayoutScale();
}

float SScaleBox::GetLayoutScale() const
{
	const EStretch::Type CurrentStretch = Stretch.Get();
	
	switch (CurrentStretch)
	{
	case EStretch::ScaleBySafeZone:
		return SafeZoneScale;
	case EStretch::UserSpecified:
		return UserSpecifiedScale.Get(1.0f);
	default:
		if ( bSingleLayoutPass )
		{
			if ( LastFinalScale.IsSet() )
			{
				return LastFinalScale.GetValue();
			}
		}

		// Because our scale is determined by our size, we always report a scale of 1.0 here, 
		// as reporting our actual scale can cause a feedback loop whereby the calculated size changes each frame.
		// We workaround this by forcibly pre-passing our child content a second time once we know its final scale in OnArrangeChildren.
		return 1.0f;
	}
}

void SScaleBox::RefreshSafeZoneScale()
{
	float ScaleDownBy = 0.f;

	if (Stretch.Get() == EStretch::ScaleBySafeZone)
	{
		TSharedPtr<SViewport> GameViewport = FSlateApplication::Get().GetGameViewport();
		if (GameViewport.IsValid())
		{
			TSharedPtr<ISlateViewport> ViewportInterface = GameViewport->GetViewportInterface().Pin();
			if (ViewportInterface.IsValid())
			{
				FIntPoint ViewportSize = ViewportInterface->GetSize();

				FDisplayMetrics Metrics;
				FSlateApplication::Get().GetDisplayMetrics(Metrics);

				// Safe zones are uniform, so the axis we check is irrelevant
#if PLATFORM_IOS
				// Hack: This is a temp solution to support iPhoneX safeArea.
				ScaleDownBy = (Metrics.TitleSafePaddingSize.X + Metrics.TitleSafePaddingSize.Y) / (float)ViewportSize.X;
#else
				ScaleDownBy = (Metrics.TitleSafePaddingSize.X * 2.f) / (float)ViewportSize.X;
#endif
			}
		}
	}

	SafeZoneScale = 1.f - ScaleDownBy;
}
