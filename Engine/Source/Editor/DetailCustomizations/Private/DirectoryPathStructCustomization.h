// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

class SButton;
class SComboButton;
class IMenu;

class FDirectoryPathStructCustomization : public IPropertyTypeCustomization
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

	/** Delegate used to display a directory picker */
	FReply OnPickDirectory(TSharedRef<IPropertyHandle> PropertyHandle, const bool bRelativeToGameContentDir, const bool bUseRelativePaths) const;

	/** Check whether that the chosen path is valid */
	bool IsValidPath(const FString& AbsolutePath, const bool bRelativeToGameContentDir, FText* const OutReason = nullptr) const;

	/** Called when a path is picked from the path picker */
	void OnPathPicked(const FString& Path, TSharedRef<IPropertyHandle> PropertyHandle);

	/** The browse button widget */
	TSharedPtr<SButton> BrowseButton;

	/** The pick button widget */
	TSharedPtr<SButton> PickerButton;

	/** The pick button popup menu*/
	TSharedPtr<IMenu> PickerMenu;
	
	/** Absolute path to the game content directory */
	FString AbsoluteGameContentDir;
};
