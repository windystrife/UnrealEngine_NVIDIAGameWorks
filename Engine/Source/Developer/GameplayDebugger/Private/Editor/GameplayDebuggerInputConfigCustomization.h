// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;

class FGameplayDebuggerInputConfigCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FGameplayDebuggerInputConfigCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

protected:
	TSharedPtr<IPropertyHandle> ConfigNameProp;
	TSharedPtr<IPropertyHandle> KeyProp;
	TSharedPtr<IPropertyHandle> ModShiftProp;
	TSharedPtr<IPropertyHandle> ModCtrlProp;
	TSharedPtr<IPropertyHandle> ModAltProp;
	TSharedPtr<IPropertyHandle> ModCmdProp;

	FText CachedHeader;
	FText GetHeaderDesc() const { return CachedHeader; }

	void OnChildValueChanged();
};

#endif // WITH_EDITOR
