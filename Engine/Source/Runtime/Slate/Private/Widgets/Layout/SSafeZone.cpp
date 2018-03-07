// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SSafeZone.h"
#include "Layout/LayoutUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"

float SSafeZone::SafeZoneScale = 1.0f;

void SSafeZone::Construct( const FArguments& InArgs )
{
	SBox::Construct(SBox::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		[
			InArgs._Content.Widget
		]
	);

	Padding = InArgs._Padding;
	SafeAreaScale = InArgs._SafeAreaScale;
	bIsTitleSafe = InArgs._IsTitleSafe;
	bPadLeft = InArgs._PadLeft;
	bPadRight = InArgs._PadRight;
	bPadTop = InArgs._PadTop;
	bPadBottom = InArgs._PadBottom;

#if WITH_EDITOR
	OverrideScreenSize = InArgs._OverrideScreenSize;
	OverrideDpiScale = InArgs._OverrideDpiScale;
#endif

	SetTitleSafe(bIsTitleSafe);

	OnSafeFrameChangedHandle = FCoreDelegates::OnSafeFrameChangedEvent.AddSP(this, &SSafeZone::SafeAreaUpdated);
}

SSafeZone::~SSafeZone()
{
	FCoreDelegates::OnSafeFrameChangedEvent.Remove(OnSafeFrameChangedHandle);
}


void SSafeZone::SetSafeZoneScale(float InScale)
{
	SafeZoneScale = InScale;

	FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
}

float SSafeZone::GetSafeZoneScale()
{
	return SafeZoneScale;
}

void SSafeZone::SafeAreaUpdated()
{
	SetTitleSafe(bIsTitleSafe);
}

void SSafeZone::SetTitleSafe( bool InIsTitleSafe )
{
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics( Metrics );

	const FMargin DeviceSafeMargin =
#if PLATFORM_IOS
		// Hack: This is a temp solution to support iPhoneX safeArea. TitleSafePaddingSize and ActionSafePaddingSize should be FVector4 and use them separately. 
		FMargin(Metrics.TitleSafePaddingSize.X, Metrics.ActionSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y, Metrics.ActionSafePaddingSize.Y);
#else
		bIsTitleSafe ?
		FMargin(Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y) :
		FMargin(Metrics.ActionSafePaddingSize.X, Metrics.ActionSafePaddingSize.Y);
#endif


#if WITH_EDITOR
	if ( OverrideScreenSize.IsSet() )
	{
		const float WidthPaddingRatio = DeviceSafeMargin.Left / ( Metrics.PrimaryDisplayWidth * 0.5f );
		const float HeightPaddingRatio = DeviceSafeMargin.Top / ( Metrics.PrimaryDisplayHeight * 0.5f );
		const FMargin OverrideSafeMargin = FMargin(WidthPaddingRatio * OverrideScreenSize->X * 0.5f, HeightPaddingRatio * OverrideScreenSize->Y * 0.5f);

		SafeMargin = OverrideSafeMargin;
	}
	else
#endif
	{
		SafeMargin = DeviceSafeMargin;
	}

#if PLATFORM_XBOXONE
	SafeMargin = SafeMargin * SafeZoneScale;
#endif

	SafeMargin = FMargin(bPadLeft ? SafeMargin.Left : 0.0f, bPadTop ? SafeMargin.Top : 0.0f, bPadRight ? SafeMargin.Right : 0.0f, bPadBottom ? SafeMargin.Bottom : 0.0f);
}

void SSafeZone::SetSidesToPad(bool InPadLeft, bool InPadRight, bool InPadTop, bool InPadBottom)
{
	bPadLeft = InPadLeft;
	bPadRight = InPadRight;
	bPadTop = InPadTop;
	bPadBottom = InPadBottom;

	SetTitleSafe(bIsTitleSafe);
}

#if WITH_EDITOR

void SSafeZone::SetOverrideScreenInformation(TOptional<FVector2D> InScreenSize, TOptional<float> InOverrideDpiScale)
{
	OverrideScreenSize = InScreenSize;
	OverrideDpiScale = InOverrideDpiScale;

	SetTitleSafe(bIsTitleSafe);
}

#endif

void SSafeZone::SetSafeAreaScale(FMargin InSafeAreaScale)
{
	SafeAreaScale = InSafeAreaScale;
}

FMargin SSafeZone::ComputeScaledSafeMargin(float Scale) const
{
#if WITH_EDITOR
	const float InvScale = OverrideDpiScale.IsSet() ? 1.0f / OverrideDpiScale.GetValue() : 1.0f / Scale;
#else
	const float InvScale = 1.0f / Scale;
#endif

	const FMargin ScaledSafeMargin(
		FMath::RoundToFloat(SafeMargin.Left * InvScale),
		FMath::RoundToFloat(SafeMargin.Top * InvScale),
		FMath::RoundToFloat(SafeMargin.Right * InvScale),
		FMath::RoundToFloat(SafeMargin.Bottom * InvScale));
	return ScaledSafeMargin;
}

void SSafeZone::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility& MyCurrentVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyCurrentVisibility ) )
	{
		const FMargin SlotPadding               = Padding.Get() + (ComputeScaledSafeMargin(AllottedGeometry.Scale) * SafeAreaScale);
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding );
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding );

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FVector2D( XAlignmentResult.Offset, YAlignmentResult.Offset ),
			FVector2D( XAlignmentResult.Size, YAlignmentResult.Size )
			)
		);
	}
}

FVector2D SSafeZone::ComputeDesiredSize(float LayoutScale) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		const FMargin SlotPadding = Padding.Get() + (ComputeScaledSafeMargin(LayoutScale) * SafeAreaScale);
		FVector2D BaseDesiredSize = SBox::ComputeDesiredSize(LayoutScale);

		return BaseDesiredSize + SlotPadding.GetDesiredSize();
	}

	return FVector2D(0, 0);
}
