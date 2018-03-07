// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateWidgetLocatorByDelegate.h"
#include "SlateWidgetElement.h"
#include "IElementLocator.h"
#include "Framework/Application/SlateApplication.h"

class FSlateWidgetLocatorByWidgetDelegate
	: public IElementLocator
{
public:

	virtual ~FSlateWidgetLocatorByWidgetDelegate()
	{ }

	virtual FString ToDebugString() const
	{
		FString DelegateName;
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
		DelegateName = Delegate.TryGetBoundFunctionName().ToString();
#endif
		return TEXT("[By::Delegate] ") + DelegateName;
	}

	virtual void Locate(TArray<TSharedRef<IApplicationElement>>& OutElements) const override
	{
		check(IsInGameThread());

		TArray<TSharedRef<SWidget>> Widgets;
		Delegate.Execute(Widgets);

		for (const TSharedRef<SWidget>& Widget : Widgets)
		{
			FWidgetPath WidgetPath;
			if (FSlateApplication::Get().FindPathToWidget(Widget, WidgetPath))
			{
				OutElements.Add(FSlateWidgetElementFactory::Create(WidgetPath));
			}
		}
	}


private:

	FSlateWidgetLocatorByWidgetDelegate(
		const FLocateSlateWidgetElementDelegate& InDelegate)
		: Delegate(InDelegate)
	{ }

private:

	FLocateSlateWidgetElementDelegate Delegate;

	friend FSlateWidgetLocatorByDelegateFactory;
};


TSharedRef<IElementLocator, ESPMode::ThreadSafe> FSlateWidgetLocatorByDelegateFactory::Create(
	const FLocateSlateWidgetElementDelegate& Delegate)
{
	return MakeShareable(new FSlateWidgetLocatorByWidgetDelegate(Delegate));
}

class FSlateWidgetLocatorByWidgetPathDelegate
	: public IElementLocator
{
public:

	virtual ~FSlateWidgetLocatorByWidgetPathDelegate()
	{ }

	virtual FString ToDebugString() const
	{
		FString DelegateName;
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
		DelegateName = Delegate.TryGetBoundFunctionName().ToString();
#endif
		return TEXT("[By::Delegate] ") + DelegateName;
	}

	virtual void Locate(TArray<TSharedRef<IApplicationElement>>& OutElements) const override
	{
		check(IsInGameThread());

		TArray<FWidgetPath> WidgetPaths;
		Delegate.Execute(WidgetPaths);

		for (const FWidgetPath& WidgetPath : WidgetPaths)
		{
			if (WidgetPath.IsValid())
			{
				OutElements.Add(FSlateWidgetElementFactory::Create(WidgetPath));
			}
		}
	}


private:

	FSlateWidgetLocatorByWidgetPathDelegate(
		const FLocateSlateWidgetPathElementDelegate& InDelegate)
		: Delegate(InDelegate)
	{ }

private:

	FLocateSlateWidgetPathElementDelegate Delegate;

	friend FSlateWidgetLocatorByDelegateFactory;
};


TSharedRef<IElementLocator, ESPMode::ThreadSafe> FSlateWidgetLocatorByDelegateFactory::Create(
	const FLocateSlateWidgetPathElementDelegate& Delegate)
{
	return MakeShareable(new FSlateWidgetLocatorByWidgetPathDelegate(Delegate));
}