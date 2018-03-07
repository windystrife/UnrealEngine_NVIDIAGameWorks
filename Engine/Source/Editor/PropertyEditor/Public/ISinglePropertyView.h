// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "PropertyEditorModule.h"

class FNotifyHook;

/**
 * Init params for a single property
 */
struct FSinglePropertyParams
{
	/** Override for the property name that will be displayed instead of the property name */
	FText NameOverride;

	/** Font to use instead of the default property font */
	FSlateFontInfo Font;

	/** Notify hook interface to use for some property change events */
	FNotifyHook* NotifyHook;

	/** Whether or not to show the name */
	EPropertyNamePlacement::Type NamePlacement;
		
	FSinglePropertyParams()
		: NameOverride(FText::GetEmpty())
		, Font()
		, NotifyHook( NULL )
		, NamePlacement( EPropertyNamePlacement::Left )
	{
	}
};


/**
 * Represents a single property not in a property tree or details view for a single object
 * Structs and Array properties cannot be used with this method
 */
class ISinglePropertyView : public SCompoundWidget
{
public:
	/** Sets the object to view/edit on the widget */
	virtual void SetObject( UObject* InObject ) = 0;

	/** Sets a delegate called when the property value changes */
	virtual void SetOnPropertyValueChanged( FSimpleDelegate& InOnPropertyValueChanged ) = 0;

	/** Whether or not this widget has a valid property */	
	virtual bool HasValidProperty() const = 0;
};
