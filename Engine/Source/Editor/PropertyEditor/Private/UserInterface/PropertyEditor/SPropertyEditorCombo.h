// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "EditorStyleSet.h"
#include "Presentation/PropertyEditor/PropertyEditor.h"
#include "UserInterface/PropertyEditor/PropertyEditorConstants.h"
#include "PropertyCustomizationHelpers.h"

class SPropertyComboBox;

class SPropertyEditorCombo : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorCombo )
		: _Font( FEditorStyle::GetFontStyle( PropertyEditorConstants::PropertyFontStyle ) ) 
		{}
		SLATE_ARGUMENT( FSlateFontInfo, Font )
		SLATE_EVENT(FOnGetPropertyComboBoxStrings, OnGetComboBoxStrings)
		SLATE_EVENT(FOnGetPropertyComboBoxValue, OnGetComboBoxValue)
		SLATE_EVENT(FOnPropertyComboBoxValueSelected, OnComboBoxValueSelected)
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, PropertyHandle)
	SLATE_END_ARGS()

	static bool Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor );

	/** Constructs widget, if InPropertyEditor is null then PropertyHandle must be set */
	void Construct( const FArguments& InArgs, const TSharedPtr< class FPropertyEditor >& InPropertyEditor = nullptr);

	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth );
private:
	void GenerateComboBoxStrings( TArray< TSharedPtr<FString> >& OutComboBoxStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems );
	void OnComboSelectionChanged( TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo );
	void OnComboOpening();

	virtual void SendToObjects( const FString& NewValue );

	/**
	 * Gets the active display value as a string
	 */
	FString GetDisplayValueAsString() const;

	/** @return True if the property can be edited */
	bool CanEdit() const;
private:

	/** Property editor this was created from, may be null */
	TSharedPtr< class FPropertyEditor > PropertyEditor;

	/** Fills out with generated strings. */
	TSharedPtr<class SPropertyComboBox> ComboBox;

	/** The property handle, will either be passed in or set from PropertyEditor */
	TSharedPtr<class IPropertyHandle> PropertyHandle;

	/** Delegate to get the strings for combo box */
	FOnGetPropertyComboBoxStrings OnGetComboBoxStrings;
	FOnGetPropertyComboBoxValue OnGetComboBoxValue;
	FOnPropertyComboBoxValueSelected OnComboBoxValueSelected;

	/**
	 * Indicates that this combo box's values are friendly names for the real values; currently only used for enum drop-downs.
	 */
	bool bUsesAlternateDisplayValues;
};
