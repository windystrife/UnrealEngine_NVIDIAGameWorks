// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

class FObjectDetails : public IDetailCustomization
{
public:
	// Creates an instance of FObjectDetails
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	// Adds a warning banner if the class is marked as Experimental or EarlyAccessPreview
	void AddExperimentalWarningCategory(IDetailLayoutBuilder& DetailBuilder);

	// Creates a button strip in each category that contains reflected functions marked as CallInEditor
	void AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder);

	// Executes the specified method on the selected objects
	FReply OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr);

private:
	// The list of selected objects, used when invoking a CallInEditor method
	TArray<TWeakObjectPtr<UObject>> SelectedObjectsList;
};
