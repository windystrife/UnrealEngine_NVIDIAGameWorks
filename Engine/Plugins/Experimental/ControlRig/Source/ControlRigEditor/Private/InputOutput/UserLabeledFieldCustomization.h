// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Reply.h"
#include "UObject/WeakObjectPtr.h"

class UK2Node_ControlRig;
class IPropertyHandleArray;

class FUserLabeledFieldCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	void HandleFieldNameSelectionChanged(TSharedPtr<FName> Value, ESelectInfo::Type SelectionInfo, TSharedPtr<IPropertyHandle> FieldNamePropertyHandle);

	void HandleFieldLabelCommitted(const FText& NewText, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> LabelPropertyHandle);

	FReply HandleMoveUp(int32 Index);

	FReply HandleMoveDown(int32 Index);

	void HandleRemove(int32 Index);

private:
	/** The ControlRigs we are currently editing */
	TArray<TWeakObjectPtr<UK2Node_ControlRig>> ControlRigs;

	/** Valid labeled field names */
	TArray<TSharedPtr<FName>> LabeledFieldNames;

	/** The array property */
	TSharedPtr<IPropertyHandleArray> PropertyHandleArray;

	/** Whether we are editing hierarchical data */
	bool bHasHierarchicalData;
};