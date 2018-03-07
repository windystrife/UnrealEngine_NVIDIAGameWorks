// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "CollectionManagerTypes.h"

class SButton;
class SComboButton;
class IMenu;

class FCollectionReferenceStructCustomization: public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:

	/** Delegate for displaying text value of path */
	FText GetDisplayedText(TSharedRef<IPropertyHandle> PropertyHandle) const;

	/** Delegate used to display a directory picker */
	FReply OnPickContent(TSharedRef<IPropertyHandle> PropertyHandle) ;

	/** Called when a path is picked from the path picker */
	void OnCollectionPicked(const FCollectionNameType& CollectionType, TSharedRef<IPropertyHandle> PropertyHandle);

	/** The pick button widget */
	TSharedPtr<SButton> PickerButton;

	/** The pick button popup menu*/
	TSharedPtr<IMenu> PickerMenu;
	};
