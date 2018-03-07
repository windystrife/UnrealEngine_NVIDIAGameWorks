// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateWidgetLocatorByUniqueTag.h"
#include "SlateWidgetElement.h"
#include "IElementLocator.h"
#include "DriverUniqueTagMetaData.h"
#include "Framework/Application/SlateApplication.h"

class FSlateWidgetLocatorByUniqueTag
	: public IElementLocator
{
public:

	virtual ~FSlateWidgetLocatorByUniqueTag()
	{ }

	virtual FString ToDebugString() const
	{
		const FString Address = FString::Printf(TEXT("0x%p"), &UniqueMetaData.Get());
		return TEXT("[UniqueTag] ") + Address;
	}

	virtual void Locate(TArray<TSharedRef<IApplicationElement>>& OutElements) const override
	{
		// Get a list of all the current slate windows
		TArray<TSharedRef<SWindow>> Windows;
		FSlateApplication::Get().GetAllVisibleWindowsOrdered(/*OUT*/Windows);

		struct FStackState
		{
			FWidgetPath Path;
		};
		TArray<FStackState> Stack;

		for (const TSharedRef<SWindow>& Window : Windows)
		{
			FStackState NewState;
			NewState.Path.Widgets.AddWidget(FArrangedWidget(Window, Window->GetWindowGeometryInScreen()));

			if (Window->GetAllMetaData<FDriverUniqueTagMetaData>().Contains(UniqueMetaData))
			{
				OutElements.Add(FSlateWidgetElementFactory::Create(NewState.Path));
				break;
			}
			else
			{
				Stack.Push(NewState);
			}
		}
		
		while (Stack.Num() > 0 && OutElements.Num() == 0)
		{
			const FStackState State = Stack.Pop();
			const FArrangedWidget& Candidate = State.Path.Widgets.Last();

			const bool bAllow3DWidgets = true;
			FArrangedChildren ArrangedChildren(VisibilityFilter, bAllow3DWidgets);
			Candidate.Widget->ArrangeChildren(Candidate.Geometry, ArrangedChildren);

			for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
			{
				const FArrangedWidget& SomeChild = ArrangedChildren[ChildIndex];

				if (SomeChild.Widget->GetAllMetaData<FDriverUniqueTagMetaData>().Contains(UniqueMetaData))
				{
					FWidgetPath NewPath = State.Path;
					NewPath.Widgets.AddWidget(SomeChild);
					OutElements.Add(FSlateWidgetElementFactory::Create(NewPath));
					break;
				}
				else
				{
					FStackState NewState = State;
					NewState.Path.Widgets.AddWidget(SomeChild);
					Stack.Push(NewState);
				}
			}
		}
	}


private:

	FSlateWidgetLocatorByUniqueTag(
		const TSharedRef<FDriverUniqueTagMetaData>& InUniqueMetaData)
		: UniqueMetaData(InUniqueMetaData)
		, VisibilityFilter(EVisibility::Visible)
	{ }

private:

	const TSharedRef<FDriverUniqueTagMetaData> UniqueMetaData;
	const EVisibility VisibilityFilter;

	friend FSlateWidgetLocatorByUniqueTagFactory;
};


TSharedRef<IElementLocator, ESPMode::ThreadSafe> FSlateWidgetLocatorByUniqueTagFactory::Create(
	const TSharedRef<FDriverUniqueTagMetaData>& UniqueMetaData)
{
	return MakeShareable(new FSlateWidgetLocatorByUniqueTag(UniqueMetaData));
}
