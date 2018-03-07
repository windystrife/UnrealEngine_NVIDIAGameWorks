// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateTypes.h"
#include "WidgetCarouselStyle.generated.h"

USTRUCT()
struct WIDGETCAROUSEL_API FWidgetCarouselNavigationButtonStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FWidgetCarouselNavigationButtonStyle();

	virtual ~FWidgetCarouselNavigationButtonStyle() {}

	virtual void GetResources(TArray<const FSlateBrush*> & OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FWidgetCarouselNavigationButtonStyle& GetDefault();

	UPROPERTY()
	FButtonStyle InnerButtonStyle;
	FWidgetCarouselNavigationButtonStyle& SetInnerButtonStyle(const FButtonStyle& InInnerButtonStyle) { InnerButtonStyle = InInnerButtonStyle; return *this; }

	UPROPERTY()
	FSlateBrush NavigationButtonLeftImage;
	FWidgetCarouselNavigationButtonStyle& SetNavigationButtonLeftImage(const FSlateBrush& InNavigationButtonLeftImage) { NavigationButtonLeftImage = InNavigationButtonLeftImage; return *this; }

	UPROPERTY()
	FSlateBrush NavigationButtonRightImage;
	FWidgetCarouselNavigationButtonStyle& SetNavigationButtonRightImage(const FSlateBrush& InNavigationButtonRightImage) { NavigationButtonRightImage = InNavigationButtonRightImage; return *this; }
};

USTRUCT(BlueprintType)
struct WIDGETCAROUSEL_API FWidgetCarouselNavigationBarStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FWidgetCarouselNavigationBarStyle();

	virtual ~FWidgetCarouselNavigationBarStyle() {}

	virtual void GetResources(TArray<const FSlateBrush*> & OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FWidgetCarouselNavigationBarStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush HighlightBrush;
	FWidgetCarouselNavigationBarStyle& SetHighlightBrush(const FSlateBrush& InHighlightBrush) { HighlightBrush = InHighlightBrush; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FButtonStyle LeftButtonStyle;
	FWidgetCarouselNavigationBarStyle& SetLeftButtonStyle(const FButtonStyle& InLeftButtonStyle) { LeftButtonStyle = InLeftButtonStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FButtonStyle CenterButtonStyle;
	FWidgetCarouselNavigationBarStyle& SetCenterButtonStyle(const FButtonStyle& InCenterButtonStyle) { CenterButtonStyle = InCenterButtonStyle; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FButtonStyle RightButtonStyle;
	FWidgetCarouselNavigationBarStyle& SetRightButtonStyle(const FButtonStyle& InRightButtonStyle) { RightButtonStyle = InRightButtonStyle; return *this; }
};

/** Manages the style which provides resources for the add content dialog. */
class WIDGETCAROUSEL_API FWidgetCarouselModuleStyle
{
public:

	static void Initialize();

	static void Shutdown();

	/** reloads textures used by slate renderer */
	static void ReloadTextures();

	/** @return The Slate style set for the widget carousel */
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > WidgetCarouselStyleInstance;
};
