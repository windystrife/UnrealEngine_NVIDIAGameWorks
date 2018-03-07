// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LocateBy.h"
#include "SlateWidgetLocatorByDelegate.h"
#include "SlateWidgetLocatorByPath.h"
#include "AutomationDriverTypeDefs.h"
#include "Framework/Application/SlateApplication.h"

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Delegate(const FLocateSlateWidgetElementDelegate& Value)
{
	return FSlateWidgetLocatorByDelegateFactory::Create(Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Delegate(const FLocateSlateWidgetPathElementDelegate& Value)
{
	return FSlateWidgetLocatorByDelegateFactory::Create(Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::WidgetLambda(const TFunction<void(TArray<TSharedRef<SWidget>>&)>& Value)
{
	return FSlateWidgetLocatorByDelegateFactory::Create(FLocateSlateWidgetElementDelegate::CreateLambda(Value));
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::WidgetPathLambda(const TFunction<void(TArray<FWidgetPath>&)>& Value)
{
	return FSlateWidgetLocatorByDelegateFactory::Create(FLocateSlateWidgetPathElementDelegate::CreateLambda(Value));
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FString& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(TEXT("#") + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FDriverElementRef& Root, const FString& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, TEXT("#") + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FName& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(TEXT("#") + Value.ToString());
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FDriverElementRef& Root, const FName& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, TEXT("#") + Value.ToString());
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const TCHAR* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(FString(TEXT("#")) + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FDriverElementRef& Root, const TCHAR* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, FString(TEXT("#")) + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const char* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(FString(TEXT("#")) + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Id(const FDriverElementRef& Root, const char* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, FString(TEXT("#")) + Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FString& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FDriverElementRef& Root, const FString& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FName& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Value.ToString());
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FDriverElementRef& Root, const FName& Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, Value.ToString());
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const TCHAR* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FDriverElementRef& Root, const TCHAR* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const char* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Path(const FDriverElementRef& Root, const char* Value)
{
	return FSlateWidgetLocatorByPathFactory::Create(Root, Value);
}

TSharedRef<IElementLocator, ESPMode::ThreadSafe> By::Cursor()
{
	return By::WidgetPathLambda([](TArray<FWidgetPath>& OutWidgetPaths) -> void {
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
		FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), Windows);

		if (WidgetPath.IsValid())
		{
			OutWidgetPaths.Add(WidgetPath);
		}
	});
}
