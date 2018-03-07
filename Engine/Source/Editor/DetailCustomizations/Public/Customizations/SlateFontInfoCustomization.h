// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/SComboBox.h"

struct FAssetData;
class IPropertyHandle;

/** Customize the appearance of an FSlateFontInfo */
class DETAILCUSTOMIZATIONS_API FSlateFontInfoStructCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;

protected:
	/** Called to filter out invalid font assets */
	static bool OnFilterFontAsset(const FAssetData& InAssetData);

	/** Called when the font object used by this FSlateFontInfo has been changed */
	void OnFontChanged(const FAssetData& InAssetData);

	/** Called to see whether the font entry combo should be enabled */
	bool IsFontEntryComboEnabled() const;

	/** Called before the font entry combo is opened - used to update the list of available font entries */
	void OnFontEntryComboOpening();

	/** Called when the selection of the font entry combo is changed */
	void OnFontEntrySelectionChanged(TSharedPtr<FName> InNewSelection, ESelectInfo::Type);

	/** Make the widget for an entry in the font entry combo */
	TSharedRef<SWidget> MakeFontEntryWidget(TSharedPtr<FName> InFontEntry);

	/** Get the text to use for the font entry combo button */
	FText GetFontEntryComboText() const;

	/** Get the name of the currently active font entry (may not be the selected entry if the entry is set to use "None") */
	FName GetActiveFontEntry() const;

	/** Get the array of FSlateFontInfo instances this customization is currently editing */
	TArray<FSlateFontInfo*> GetFontInfoBeingEdited();
	TArray<const FSlateFontInfo*> GetFontInfoBeingEdited() const;

	/** Handle to the struct property being edited */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Handle to the "FontObject" property being edited */
	TSharedPtr<IPropertyHandle> FontObjectProperty;

	/** Handle to the "TypefaceFontName" property being edited */
	TSharedPtr<IPropertyHandle> TypefaceFontNameProperty;

	/** Handle to the "Size" property being edited */
	TSharedPtr<IPropertyHandle> FontSizeProperty;

	/** Font entry combo box widget */
	TSharedPtr<SComboBox<TSharedPtr<FName>>> FontEntryCombo;

	/** Source data for the font entry combo widget */
	TArray<TSharedPtr<FName>> FontEntryComboData;
};
