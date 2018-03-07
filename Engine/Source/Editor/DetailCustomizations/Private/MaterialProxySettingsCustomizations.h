// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "IPropertyTypeCustomization.h"

class FDetailWidgetRow;
class IPropertyHandle;

class FMaterialProxySettingsCustomizations : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization instance */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
protected:
	void AddTextureSizeClamping(TSharedPtr<IPropertyHandle> TextureSizeProperty);
protected:
	EVisibility AreManualOverrideTextureSizesEnabled() const;
	EVisibility IsTextureSizeEnabled() const;

	EVisibility IsSimplygonMaterialMergingVisible() const;
	
	TSharedPtr< IPropertyHandle > EnumHandle;

	TSharedPtr< IPropertyHandle > TextureSizeHandle;
	TArray<TSharedPtr<IPropertyHandle>> PropertyTextureSizeHandles;

	TSharedPtr< IPropertyHandle > MergeTypeHandle;
	TSharedPtr< IPropertyHandle > GutterSpaceHandle;
};
