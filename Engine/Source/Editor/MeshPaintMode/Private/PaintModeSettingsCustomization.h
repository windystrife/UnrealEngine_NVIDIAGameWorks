// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "PropertyRestriction.h"
#include "Widgets/SWidget.h"

class UPaintModeSettings;
class FPaintModePainter;
class SHorizontalBox;
struct FVertexPaintSettings;
struct FTexturePaintSettings;

class FPaintModeSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	void OnPaintTypeChanged(IDetailLayoutBuilder* LayoutBuilder);
};

class FPaintModeSettingsRootObjectCustomization : public IDetailRootObjectCustomization
{
public:
	/** IDetailRootObjectCustomization interface */
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) override;
	virtual bool IsObjectVisible(const UObject* InRootObject) const override { return true; }
	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const override { return false; }
};

class FVertexPaintSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {};
protected:
	/** Checks whether or not vertex painting settings are visible (depends whether or not in color or blend weight painting mode) */
	EVisibility ArePropertiesVisible(const int32 VisibleType) const;

	/** Callback for when texture weight type changed so we can update restrictions */
	void OnTextureWeightTypeChanged(TSharedRef<IPropertyHandle> WeightTypeProperty, TSharedRef<IPropertyHandle> PaintWeightProperty, TSharedRef<IPropertyHandle> EraseWeightProperty);
protected:
	/** Cached instance of paint mode settings */
	UPaintModeSettings* Settings;

	/** Cached instance of vertex paint settings */
	FVertexPaintSettings* PaintSettings;	

	/** Property restriction applied to blend paint enum dropdown box */
	TSharedPtr<FPropertyRestriction> BlendPaintEnumRestriction;

	/** Static list of property names which require customization */
	static TArray<FName> CustomPropertyNames;
};

class FTexturePaintSettingsCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override {};
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
protected:
	/** Cached instance of the painter */
	FPaintModePainter* MeshPainter;
	/** Cached instance of the texture painting settings */
	FTexturePaintSettings* PaintSettings;
};

/** Creates a widget representing a color channel */
static TSharedRef<SHorizontalBox> CreateColorChannelWidget(TSharedRef<IPropertyHandle> ChannelProperty);

	