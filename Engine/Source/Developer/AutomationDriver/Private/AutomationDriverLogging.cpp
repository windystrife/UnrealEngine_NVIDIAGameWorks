// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomationDriverLogging.h"
#include "AutomationDriverCommon.h"
#include "IApplicationElement.h"

DEFINE_LOG_CATEGORY(LogAutomationDriver);

void FAutomationDriverLogging::TooManyElementsFound(const TArray<TSharedRef<IApplicationElement>>& Elements)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Multiple elements found when 1 was expected\nExpected 1\n   Found %d"), Elements.Num());

	for (int32 Index = 0; Index < Elements.Num(); Index++)
	{
		const TSharedRef<IApplicationElement>& Element = Elements[Index];
		UE_LOG(LogAutomationDriver, Error, TEXT("    [%d] -> %s"), Index, *Element->ToDebugString());
	}
}

void FAutomationDriverLogging::CannotFindElement(const TSharedPtr<IElementLocator, ESPMode::ThreadSafe>& ElementLocator)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Failed to locate element"));

	if (ElementLocator.IsValid())
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    %s"), *ElementLocator->ToDebugString());
	}
}

void FAutomationDriverLogging::ElementNotVisible(const TSharedPtr<IElementLocator, ESPMode::ThreadSafe>& ElementLocator)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Failed to locate visible element"));

	if (ElementLocator.IsValid())
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not visible: %s"), *ElementLocator->ToDebugString());
	}
	else
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not visible"));
	}
}

void FAutomationDriverLogging::ElementNotInteractable(const TSharedPtr<IElementLocator, ESPMode::ThreadSafe>& ElementLocator)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Failed to locate interactable element"));

	if (ElementLocator.IsValid())
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not interactable: %s"), *ElementLocator->ToDebugString());
	}
	else
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not interactable"));
	}
}

void FAutomationDriverLogging::ElementHasNoWindow(const TSharedPtr<IElementLocator, ESPMode::ThreadSafe>& ElementLocator)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Failed to locate window hosting element"));

	if (ElementLocator.IsValid())
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but no window is associated with it: %s"), *ElementLocator->ToDebugString());
	}
	else
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but no window is associated with it"));
	}
}

void FAutomationDriverLogging::CannotClickUnhoveredElement(const TSharedPtr<IElementLocator, ESPMode::ThreadSafe>& ElementLocator)
{
	UE_LOG(LogAutomationDriver, Error, TEXT("Failed to click element"));

	if (ElementLocator.IsValid())
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not located under the cursor: %s"), *ElementLocator->ToDebugString());
	}
	else
	{
		UE_LOG(LogAutomationDriver, Error, TEXT("    Element found but not located under the cursor"));
	}
}

