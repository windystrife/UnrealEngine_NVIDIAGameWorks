// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "AssetData.h"
#include "DetailWidgetRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "PropertyHandle.h"

class IPropertyHandle;

class FGameplayTagReferenceHelperDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:

	struct FGameplayTagReferenceTreeItem : public TSharedFromThis<FGameplayTagReferenceTreeItem>
	{
		FGameplayTagReferenceTreeItem()
		{
		}

		FName GameplayTagName;
		FAssetIdentifier AssetIdentifier;	
	};

	typedef STreeView< TSharedPtr< FGameplayTagReferenceTreeItem > > SGameplayTagReferenceTree;

	TArray< TSharedPtr<FGameplayTagReferenceTreeItem> > TreeItems;

	TSharedRef<ITableRow> OnGenerateWidgetForGameplayCueListView(TSharedPtr< FGameplayTagReferenceTreeItem > InItem, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<IPropertyHandle> StatBackendNameProp;

	TSharedPtr<class IPropertyHandle> PropertyHandle;

	struct FGameplayTagReferenceHelper* GetValue();
};

class FGameplayTagCreationWidgetHelperDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              
	TSharedPtr<class SGameplayTagWidget> TagWidget;
};