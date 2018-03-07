// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "IDetailCustomization.h"

class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SEditableTextBox;

class FRigDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:

	/** Generate a widget for a movie array element */
	void GenerateNodeArrayElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout);
	void GenerateTransformBaseArrayElementWidget(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout);

	TSharedPtr<IPropertyHandle> NodesPropertyHandle;
	TSharedPtr<IPropertyHandle> TransformBasesPropertyHandle;

	// array custom boxes - these will stay around as long as this window is up
	TArray<TSharedPtr<SEditableTextBox>>	DisplayNameTextBoxes;
	TIndirectArray<TArray<TSharedPtr<FString>>>		ParentSpaceOptionList;
	TArray<TSharedPtr<SComboBox< TSharedPtr<FString> >>>	ParentSpaceComboBoxes;

	/** we only support one item */
	TWeakObjectPtr<UObject> ItemBeingEdited;

	// node display text handler
	void ValidErrorMessage(const FString& DisplayString, int32 ArrayIndex);
	FText GetDisplayName(TSharedRef<IPropertyHandle> DisplayNameProp) const;
	void OnDisplayNameChanged(const FText& Text, TSharedRef<IPropertyHandle> DisplayNameProp, int32 ArrayIndex);
	void OnDisplayNameCommitted(const FText& Text, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> DisplayNameProp, int32 ArrayIndex);

	// combo box handler
	void OnParentSpaceSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentSpacePropertyHandle);
	/** Called to create a widget for each string */
	TSharedRef<SWidget> MakeItemWidget(TSharedPtr<FString> StringItem);
	// Helper method to get the text for a given item in the combo box 
	FText GetSelectedTextLabel(TSharedRef<IPropertyHandle> ParentSpacePropertyHandle) const;
	void OnComboBoxOopening(TSharedRef<IPropertyHandle> ParentSpacePropertyHandle, int32 ArrayIndex, bool bTranslation);
	// check box handler
	// Callback for changing this row's Share check box state.
	void OnAdvancedCheckBoxStateChanged(ECheckBoxState NewState, TSharedRef<IPropertyHandle> PropertyHandle);
	ECheckBoxState AdvancedCheckBoxIsChecked(TSharedRef<IPropertyHandle> PropertyHandle) const;

	// button handlers
	FReply OnSetAllToWorld();
	FReply OnSetAllToParent();

	IDetailLayoutBuilder * DetailBuilderPtr;
};

