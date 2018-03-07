// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class SErrorText;

class DETAILCUSTOMIZATIONS_API FSlateBrushStructCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance(bool bIncludePreview);

	FSlateBrushStructCustomization(bool bIncludePreview);

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:
	/**
	 * Get the Slate Brush tiling property row visibility
	 */
	EVisibility GetTilingPropertyVisibility() const;

	/**
	 *  Get the Slate Brush margin property row visibility
	 */
	EVisibility GetMarginPropertyVisibility() const;

	/** Callback for determining image size reset button visibility */
	bool IsImageSizeResetToDefaultVisible(TSharedPtr<IPropertyHandle> PropertyHandle) const;

	/** Callback for clicking the image size reset button */
	void OnImageSizeResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle) const;

	/** Gets the current default image size based on the current texture resource */
	FVector2D GetDefaultImageSize() const;

	/** Slate Brush DrawAs property */
	TSharedPtr<IPropertyHandle> DrawAsProperty;

/** Slate Brush Image Size property */
	TSharedPtr<IPropertyHandle> ImageSizeProperty;

/** Slate Brush Resource Object property */
	TSharedPtr<IPropertyHandle> ResourceObjectProperty;

	/** Error text to display if the resource object is not valid*/
	TSharedPtr<SErrorText> ResourceErrorText;

	/** Should we show the preview portion of the customization? */
	bool bIncludePreview;
};

