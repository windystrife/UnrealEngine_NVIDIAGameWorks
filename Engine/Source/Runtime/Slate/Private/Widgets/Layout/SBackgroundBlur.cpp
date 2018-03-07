// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "SBackgroundBlur.h"
#include "DrawElements.h"
#include "IConsoleManager.h"

static int32 bAllowBackgroundBlur = 1;
static FAutoConsoleVariableRef CVarSlateAllowBackgroundBlurWidgets(TEXT("Slate.AllowBackgroundBlurWidgets"), bAllowBackgroundBlur, TEXT("If 0, no background blur widgets will be rendered"));

static int32 MaxKernelSize = 255;
static FAutoConsoleVariableRef CVarSlateMaxKernelSize(TEXT("Slate.BackgroundBlurMaxKernelSize"), MaxKernelSize, TEXT("The maximum allowed kernel size.  Note: Very large numbers can cause a huge decrease in performance"));

static int32 bDownsampleForBlur = 1;
static FAutoConsoleVariableRef CVarDownsampleForBlur(TEXT("Slate.BackgroundBlurDownsample"), bDownsampleForBlur, TEXT(""), ECVF_Cheat);

#if PLATFORM_ANDROID
// This feature has not been tested on es2 and will likely not work so we force low quality fallback mode
static int32 bForceLowQualityBrushFallback = 1;
#else
static int32 bForceLowQualityBrushFallback = 0;
#endif


static FAutoConsoleVariableRef CVarForceLowQualityBackgroundBlurOverride(TEXT("Slate.ForceBackgroundBlurLowQualityOverride"), bForceLowQualityBrushFallback, TEXT("Whether or not to force a slate brush to be used instead of actually blurring the background"), ECVF_Scalability);


void SBackgroundBlur::Construct(const FArguments& InArgs)
{
	bApplyAlphaToBlur = InArgs._bApplyAlphaToBlur;
	LowQualityFallbackBrush = InArgs._LowQualityFallbackBrush;
	BlurStrength = InArgs._BlurStrength;
	BlurRadius = InArgs._BlurRadius;

	ChildSlot
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
	[
		InArgs._Content.Widget
	];
}

void SBackgroundBlur::SetContent(const TSharedRef<SWidget>& InContent)
{
	ChildSlot.AttachWidget(InContent);
}

void SBackgroundBlur::SetApplyAlphaToBlur(bool bInApplyAlphaToBlur)
{
	bApplyAlphaToBlur = bInApplyAlphaToBlur;
	Invalidate(EInvalidateWidget::Layout);
}

void SBackgroundBlur::SetBlurRadius(const TAttribute<TOptional<int32>>& InBlurRadius)
{
	BlurRadius = InBlurRadius;
	Invalidate(EInvalidateWidget::Layout);
}

void SBackgroundBlur::SetBlurStrength(const TAttribute<float>& InStrength)
{
	BlurStrength = InStrength;
	Invalidate(EInvalidateWidget::Layout);
}

void SBackgroundBlur::SetLowQualityBackgroundBrush(const FSlateBrush* InBrush)
{
	LowQualityFallbackBrush = InBrush;
	Invalidate(EInvalidateWidget::Layout);
}

void SBackgroundBlur::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SBackgroundBlur::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SBackgroundBlur::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

bool SBackgroundBlur::IsUsingLowQualityFallbackBrush() const
{
	return bForceLowQualityBrushFallback == 1;
}

int32 SBackgroundBlur::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 PostFXLayerId = LayerId;
	if (bAllowBackgroundBlur && AllottedGeometry.GetLocalSize() > FVector2D::ZeroVector)
	{
		if (!bForceLowQualityBrushFallback)
		{
			// Modulate blur strength by the widget alpha
			const float Strength = BlurStrength.Get() * (bApplyAlphaToBlur ? InWidgetStyle.GetColorAndOpacityTint().A : 1.f);
			if (Strength > 0.f)
			{
				FSlateRect RenderBoundingRect = AllottedGeometry.GetRenderBoundingRect();
				FPaintGeometry PaintGeometry(RenderBoundingRect.GetTopLeft(), RenderBoundingRect.GetSize(), 1.0f);

				int32 RenderTargetWidth = FMath::RoundToInt(RenderBoundingRect.GetSize().X);
				int32 RenderTargetHeight = FMath::RoundToInt(RenderBoundingRect.GetSize().Y);

				int32 KernelSize = 0;
				int32 DownsampleAmount = 0;
				ComputeEffectiveKernelSize(Strength, KernelSize, DownsampleAmount);

				float ComputedStrength = FMath::Max(.5f, Strength);

				if (DownsampleAmount > 0)
				{
					RenderTargetWidth = FMath::DivideAndRoundUp(RenderTargetWidth, DownsampleAmount);
					RenderTargetHeight = FMath::DivideAndRoundUp(RenderTargetHeight, DownsampleAmount);
					ComputedStrength /= DownsampleAmount;
				}

				if (RenderTargetWidth > 0 && RenderTargetHeight > 0)
				{
					OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));

					FSlateDrawElement::MakePostProcessPass(OutDrawElements, LayerId, PaintGeometry, FVector4(KernelSize, ComputedStrength, RenderTargetWidth, RenderTargetHeight), DownsampleAmount);

					OutDrawElements.PopClip();
				}

				++PostFXLayerId;
			}
		}
		else if (bAllowBackgroundBlur && bForceLowQualityBrushFallback && LowQualityFallbackBrush && LowQualityFallbackBrush->DrawAs != ESlateBrushDrawType::NoDrawType)
		{
			const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
			const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

			const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint() * LowQualityFallbackBrush->GetTint(InWidgetStyle));

			FSlateDrawElement::MakeBox(OutDrawElements, PostFXLayerId, AllottedGeometry.ToPaintGeometry(), LowQualityFallbackBrush, DrawEffects, FinalColorAndOpacity);
			++PostFXLayerId;
		}
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, PostFXLayerId, InWidgetStyle, bParentEnabled);
}

void SBackgroundBlur::ComputeEffectiveKernelSize(float Strength, int32& OutKernelSize, int32& OutDownsampleAmount) const
{
	// If the radius isn't set, auto-compute it based on the strength
	OutKernelSize = BlurRadius.Get().Get(FMath::RoundToInt(Strength * 3.f));

	// Downsample if needed
	if (bDownsampleForBlur && OutKernelSize > 9)
	{
		OutDownsampleAmount = OutKernelSize >= 64 ? 4 : 2;
		OutKernelSize /= OutDownsampleAmount;
	}

	// Kernel sizes must be odd
	if (OutKernelSize % 2 == 0)
	{
		++OutKernelSize;
	}

	OutKernelSize = FMath::Clamp(OutKernelSize, 3, MaxKernelSize);
}