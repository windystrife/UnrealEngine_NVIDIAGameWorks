// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;

class FGameplayDebuggerCategoryConfigCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FGameplayDebuggerCategoryConfigCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:
	TSharedPtr<IPropertyHandle> CategoryNameProp;
	TSharedPtr<IPropertyHandle> SlotIdxProp;
	TSharedPtr<IPropertyHandle> ActiveInGameProp;
	TSharedPtr<IPropertyHandle> ActiveInSimulateProp;

	FText CachedHeader;
	FText GetHeaderDesc() const { return CachedHeader; }

	void OnChildValueChanged();
};

#endif // WITH_EDITOR
