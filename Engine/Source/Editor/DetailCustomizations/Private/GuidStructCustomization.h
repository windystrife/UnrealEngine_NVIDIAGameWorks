// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class SEditableTextBox;

namespace EPropertyEditorGuidActions
{
	/**
	 * Enumerates quick-set action types.
	 */
	enum Type
	{
		/** Generate a new GUID. */
		Generate,

		/** Set a null GUID. */
		Invalidate
	};
}


/**
 * Implements a details panel customization for FGuid structures.
 */
class FGuidStructCustomization
	: public IPropertyTypeCustomization
{
public:

	/**
	 * Creates a new instance.
	 *
	 * @return A new struct customization for Guids.
	 */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance( )
	{
		return MakeShareable(new FGuidStructCustomization);
	}

public:

	// IPropertyTypeCustomization interface

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

protected:

	/**
	 * Sets the property's value.
	 *
	 * @param Guid The value to set.
	 */
	void SetGuidValue( const FGuid& Guid );

private:

	/** Callback for clicking an item in the quick-set menu. */
	void HandleGuidActionClicked( EPropertyEditorGuidActions::Type Action );

	/** Handles getting the text color of the editable text box. */
	FSlateColor HandleTextBoxForegroundColor( ) const;

	/** Handles getting the text to be displayed in the editable text box. */
	FText HandleTextBoxText( ) const;

	/** Handles changing the value in the editable text box. */
	void HandleTextBoxTextChanged( const FText& NewText );

	/** Handles committing the text in the editable text box. */
	void HandleTextBoxTextCommited( const FText& NewText, ETextCommit::Type CommitInfo );

private:

	/** Holds a flag indicating whether the current input is a valid GUID. */
	bool InputValid;

	/** Holds a handle to the property being edited. */
	TSharedPtr<IPropertyHandle> PropertyHandle;

	/** Holds the text box for editing the Guid. */
	TSharedPtr<SEditableTextBox> TextBox;
};
