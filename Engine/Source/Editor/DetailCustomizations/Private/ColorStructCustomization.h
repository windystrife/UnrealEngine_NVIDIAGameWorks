// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "MathStructCustomizations.h"

class FDetailWidgetRow;
class SColorPicker;

/**
 * Base class for color struct customization (FColor,FLinearColor).
 */
class FColorStructCustomization
	: public FMathStructCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	
protected:

	FColorStructCustomization()
		: bIgnoreAlpha(false)
		, bIsInlineColorPickerVisible(false)
		, bIsInteractive(false)
		, bDontUpdateWhileEditing(false)
	{}

protected:

	/** Creates the color widget that when clicked spawns the color picker window. */
	TSharedRef<SWidget> CreateColorWidget(TWeakPtr<IPropertyHandle>);

	/**
	 * Get the color used by this struct as a linear color value
	 * @param InColor To be filled with the color value used by this struct, or white if this struct is being used to edit multiple values  
	 * @return The result of trying to get the color value
	 */
	FPropertyAccess::Result GetColorAsLinear(FLinearColor& InColor) const;

	/**
	 * Does this struct have multiple values?
	 * @return EVisibility::Visible if it does, EVisibility::Collapsed otherwise
	 */
	EVisibility GetMultipleValuesTextVisibility() const;

	/**
	 * Creates a new color picker for interactively selecting the color
	 *
	 * @param bUseAlpha If true alpha will be displayed, otherwise it is ignored
	 * @param bOnlyRefreshOnOk If true the value of the property will only be refreshed when the user clicks OK in the color picker
	 */
	void CreateColorPicker(bool bUseAlpha);
	
	/** Creates a new color picker for interactively selecting color */
	TSharedRef<SColorPicker> CreateInlineColorPicker(TWeakPtr<IPropertyHandle>);

	/**
	 * Called when the property is set from the color picker 
	 * 
	 * @param NewColor The new color value
	 */
	void OnSetColorFromColorPicker(FLinearColor NewColor);
	
	/**
	 * Called when the user clicks cancel in the color picker
	 * The values are reset to their original state when this happens
	 *
	 * @param OriginalColor Original color of the property
	 */
	void OnColorPickerCancelled(FLinearColor OriginalColor);

	/**
	 * Called when the user enters an interactive color change (dragging something in the picker)
	 */
	void OnColorPickerInteractiveBegin();

	/**
	 * Called when the user completes an interactive color change (dragging something in the picker)
	 */
	void OnColorPickerInteractiveEnd();

	/**
	 * @return The color that should be displayed in the color block                                                              
	 */
	FLinearColor OnGetColorForColorBlock() const;
	
	/**
	 * Called when the user clicks in the color block (opens inline color picker)
	 */
	FReply OnMouseButtonDownColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
	/** Called when the user clicks on the the button to get the full color picker */
	FReply OnOpenFullColorPickerClicked();

protected:

	// FMathStructCustomization interface

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void MakeHeaderRow(TSharedRef<IPropertyHandle>& InStructPropertyHandle, FDetailWidgetRow& Row) override;
	virtual void GetSortedChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, TArray< TSharedRef<IPropertyHandle> >& OutChildren) override;

protected:
	/** Stores a linear or srb color without converting between the two. Only one is valid at a time */
	struct FLinearOrSrgbColor
	{
		FLinearOrSrgbColor(const FLinearColor& InLinearColor)
			: LinearColor(InLinearColor)
		{}

		FLinearOrSrgbColor(const FColor& InSrgbColor)
			: SrgbColor(InSrgbColor)
		{}

		FLinearColor GetLinear() const { return LinearColor; }
		FColor GetSrgb() const { return SrgbColor; }
	private:
		FLinearColor LinearColor;
		FColor SrgbColor;
	};

	/** Saved per struct colors in case the user clicks cancel in the color picker */
	TArray<FLinearOrSrgbColor> SavedPreColorPickerColors;

	/** Color struct handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** True if the property is a linear color property */
	bool bIsLinearColor;

	/** True if the property wants to ignore the alpha component */
	bool bIgnoreAlpha;

	/** True if the inline color picker is visible */
	bool bIsInlineColorPickerVisible;

	/** True if the user is performing an interactive color change */
	bool bIsInteractive;

	/** Cached widget for the color picker to use as a parent */
	TSharedPtr<SWidget> ColorPickerParentWidget;

	/** The value won;t be updated while editing */
	bool bDontUpdateWhileEditing;

	/** Overrides the default state of the sRGB check box */
	TOptional<bool> sRGBOverride;
};
