// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Widgets/SCompoundWidget.h"
/**
* A BackgroundBlur is similar to a Border in that it can be used to contain other widgets.
* However, instead of surrounding the content with an image, it applies a post-process blur to
* everything (both actors and widgets) that is underneath it.
* 
* Note: For low-spec machines where the blur effect is too computationally expensive, 
* a user-specified fallback image is used instead (effectively turning this into a Border)
*/
class SLATE_API SBackgroundBlur : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBackgroundBlur)
		: _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(2.0f))
		, _bApplyAlphaToBlur(true)
		, _BlurStrength(0.f)
		, _BlurRadius()
		, _LowQualityFallbackBrush(nullptr)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)

		SLATE_ARGUMENT(bool, bApplyAlphaToBlur)
		SLATE_ATTRIBUTE(float, BlurStrength)
		SLATE_ATTRIBUTE(TOptional<int32>, BlurRadius)
		SLATE_ARGUMENT(const FSlateBrush*, LowQualityFallbackBrush)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void SetContent(const TSharedRef<SWidget>& InContent);
	void SetApplyAlphaToBlur(bool bInApplyAlphaToBlur);
	void SetBlurRadius(const TAttribute<TOptional<int32>>& InBlurRadius);
	void SetBlurStrength(const TAttribute<float>& InStrength);
	void SetLowQualityBackgroundBrush(const FSlateBrush* InBrush);
	
	void SetHAlign(EHorizontalAlignment HAlign);
	void SetVAlign(EVerticalAlignment VAlign);
	void SetPadding(const TAttribute<FMargin>& InPadding);

	bool IsUsingLowQualityFallbackBrush() const;

protected:
	void ComputeEffectiveKernelSize(float Strength, int32& OutKernelSize, int32& OutDownsampleAmount) const;

	bool bApplyAlphaToBlur;
	TAttribute<float> BlurStrength;
	TAttribute<TOptional<int32>> BlurRadius;
	const FSlateBrush* LowQualityFallbackBrush;
};