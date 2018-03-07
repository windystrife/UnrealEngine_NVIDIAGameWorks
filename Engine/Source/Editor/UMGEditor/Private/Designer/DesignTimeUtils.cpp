// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Designer/DesignTimeUtils.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

bool FDesignTimeUtils::GetArrangedWidget(TSharedRef<SWidget> Widget, FArrangedWidget& ArrangedWidget)
{
	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(Widget);
	if ( !WidgetWindow.IsValid() )
	{
		return false;
	}

	TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

	FWidgetPath WidgetPath;
	if ( FSlateApplication::Get().GeneratePathToWidgetUnchecked(Widget, WidgetPath) )
	{
		ArrangedWidget = WidgetPath.FindArrangedWidget(Widget).Get(FArrangedWidget::NullWidget);
		return true;
	}

	return false;
}

bool FDesignTimeUtils::GetArrangedWidgetRelativeToWindow(TSharedRef<SWidget> Widget, FArrangedWidget& ArrangedWidget)
{
	TSharedPtr<SWindow> WidgetWindow = FSlateApplication::Get().FindWidgetWindow(Widget);
	if ( !WidgetWindow.IsValid() )
	{
		return false;
	}

	TSharedRef<SWindow> CurrentWindowRef = WidgetWindow.ToSharedRef();

	FWidgetPath WidgetPath;
	if ( FSlateApplication::Get().GeneratePathToWidgetUnchecked(Widget, WidgetPath) )
	{
		ArrangedWidget = WidgetPath.FindArrangedWidget(Widget).Get(FArrangedWidget::NullWidget);
		ArrangedWidget.Geometry.AppendTransform(FSlateLayoutTransform(Inverse(CurrentWindowRef->GetPositionInScreen())));
		//ArrangedWidget.Geometry.AppendTransform(Inverse(CurrentWindowRef->GetLocalToScreenTransform()));
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
