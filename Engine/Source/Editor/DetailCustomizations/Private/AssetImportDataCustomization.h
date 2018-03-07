// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IPropertyTypeCustomization.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IPropertyHandle;
class UAssetImportData;
struct FAssetImportInfo;

class FAssetImportDataCustomization : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils ) override;

private:

	/** Handle the user wanting to change a specific source path */
	FReply OnChangePathClicked(int32 Index) const;

	/** Handle the user requesting that the specified index be cleared */
	FReply OnClearPathClicked(int32 Index) const;

	/** Access the struct we are editing */
	FAssetImportInfo* GetEditStruct() const;

	/** Access the outer class that contains this struct */
	UAssetImportData* GetOuterClass() const;

	/** Get text for the UI */
	FText GetFilenameText(int32 Index) const;
	FText GetTimestampText(int32 Index) const;

private:

	/** Property handle of the property we're editing */
	TSharedPtr<IPropertyHandle> PropertyHandle;
};
