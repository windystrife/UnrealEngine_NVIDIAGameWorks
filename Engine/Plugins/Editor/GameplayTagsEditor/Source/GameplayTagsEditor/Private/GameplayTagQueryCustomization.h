// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Editor/PropertyEditor/Public/IPropertyTypeCustomization.h"
#include "SGameplayTagQueryWidget.h"
#include "EditorUndoClient.h"

class IPropertyHandle;
class SWindow;

/** Customization for the gameplay tag query struct */
class FGameplayTagQueryCustomization : public IPropertyTypeCustomization, public FEditorUndoClient
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FGameplayTagQueryCustomization);
	}

	~FGameplayTagQueryCustomization();

	/** Overridden to show an edit button to launch the gameplay tag editor */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}

private:
	/** Called when the edit button is clicked; Launches the gameplay tag editor */
	FReply OnEditButtonClicked();

	/** Overridden to do nothing */
	FText GetEditButtonText() const;

	/** Called when the "clear all" button is clicked */
	FReply OnClearAllButtonClicked();

	/** Returns the visibility of the "clear all" button (collapsed when there are no tags) */
	EVisibility GetClearAllVisibility() const;

	/** Returns the visibility of the tags list box (collapsed when there are no tags) */
	EVisibility GetQueryDescVisibility() const;

	void RefreshQueryDescription();

	FText GetQueryDescText() const;

	void CloseWidgetWindow(bool WasCancelled);

	/** Build List of Editable Queries */
	void BuildEditableQueryList();

	/** Cached property handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** The array of queries this objects has */
	TArray<SGameplayTagQueryWidget::FEditableGameplayTagQueryDatum> EditableQueries;

	/** The Window for the GameplayTagWidget */
	TSharedPtr<SWindow> GameplayTagQueryWidgetWindow;

	FString QueryDescription;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};

