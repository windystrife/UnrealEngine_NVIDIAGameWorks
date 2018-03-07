// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailRootObjectCustomization.h"
#include "SListView.h"

class FClothPainter;
class UClothingAsset;
class UClothPainterSettings;
class FClothPaintToolBase;

class FClothPaintSettingsCustomization : public IDetailCustomization
{
public:

	FClothPaintSettingsCustomization() = delete;
	FClothPaintSettingsCustomization(FClothPainter* InPainter);

	virtual ~FClothPaintSettingsCustomization();

	static TSharedRef<IDetailCustomization> MakeInstance(FClothPainter* InPainter);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	/** Handlers for SCombobox, generation of row widgets, combo text and handling selection change */
	TSharedRef<SWidget> OnGenerateToolComboRow(TSharedPtr<FClothPaintToolBase> InItem);
	void OnHandleToolSelection(TSharedPtr<FClothPaintToolBase> InItem, ESelectInfo::Type InSelectInfo, IDetailLayoutBuilder* DetailBuider);
	FText GetToolComboText() const;

	/** The painter containing the paint settings we are customizing */
	FClothPainter* Painter;
};


class FClothPaintSettingsRootObjectCustomization : public IDetailRootObjectCustomization
{
public:

	/** IDetailRootObjectCustomization interface */
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) override { return SNullWidget::NullWidget; }
	virtual bool IsObjectVisible(const UObject* InRootObject) const override { return true; }
	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const override { return false; }
};


class FClothPaintBrushSettingsCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
