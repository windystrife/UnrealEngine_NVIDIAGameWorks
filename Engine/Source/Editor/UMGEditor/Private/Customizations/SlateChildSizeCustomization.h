// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Components/SlateWrapperTypes.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class FSlateChildSizeCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FSlateChildSizeCustomization());
	}

	FSlateChildSizeCustomization()
	{
	}
	
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	void HandleCheckStateChanged(ECheckBoxState InCheckboxState, TSharedPtr<IPropertyHandle> PropertyHandle, ESlateSizeRule::Type ToRule);

	ECheckBoxState GetCheckState(TSharedPtr<IPropertyHandle> PropertyHandle, ESlateSizeRule::Type ForRule) const;

	TOptional<float> GetValue(TSharedPtr<IPropertyHandle> ValueHandle) const;

	void HandleValueComitted(float NewValue, ETextCommit::Type CommitType, TSharedPtr<IPropertyHandle> ValueHandle);

	EVisibility GetValueVisiblity(TSharedPtr<IPropertyHandle> RuleHandle) const;
};
