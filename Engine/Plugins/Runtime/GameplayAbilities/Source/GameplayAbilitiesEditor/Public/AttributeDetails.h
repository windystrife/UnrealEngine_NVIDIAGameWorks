// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class SComboButton;

DECLARE_LOG_CATEGORY_EXTERN(LogAttributeDetails, Log, All);

class FAttributeDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	TSharedPtr<FString> GetPropertyType() const;

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<IPropertyHandle> MyProperty;

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);	
};


class FAttributePropertyDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:

	// the attribute property
	TSharedPtr<IPropertyHandle> MyProperty;
	// the owner property
	TSharedPtr<IPropertyHandle> OwnerProperty;
	// the name property
	TSharedPtr<IPropertyHandle> NameProperty;

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<FString>	GetPropertyType() const;

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void OnAttributeChanged(UProperty* SelectedAttribute);
};

class GAMEPLAYABILITIESEDITOR_API FScalableFloatDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FScalableFloatDetails()
		: PreviewLevel(0.f)
		, MaxPreviewLevel(30.f) // This should perhaps be configurable per scalable float somehow
	{
	}

protected:

	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

	bool IsEditable( ) const;

	virtual TSharedRef<SWidget> CreateCurveTableWidget();

	EVisibility GetRowNameVisibility() const;
	EVisibility GetPreviewVisibility() const;
	float GetPreviewLevel() const;
	void SetPreviewLevel(float NewLevel);

	TSharedRef<SWidget> GetListContent();
	void OnSelectionChanged(TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo);
	FText GetRowNameComboBoxContentText() const;
	FText GetRowValuePreviewLabel() const;
	FText GetRowValuePreviewText() const;
	TSharedRef<ITableRow> HandleRowNameComboBoxGenarateWidget(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnFilterTextChanged(const FText& InFilterText);
	
	class UCurveTable * GetCurveTable();

	TSharedPtr<FString> InitWidgetContent();

	void OnCurveTableChanged();

	TSharedPtr<SComboButton> RowNameComboButton;
	TSharedPtr<SListView<TSharedPtr<FString> > > RowNameComboListView;
	TSharedPtr<FString> CurrentSelectedItem;
	TArray<TSharedPtr<FString> > RowNames;

	TSharedPtr<IPropertyHandle> ValueProperty;
	TSharedPtr<IPropertyHandle> CurveTableHandleProperty;
	TSharedPtr<IPropertyHandle> CurveTableProperty;
	TSharedPtr<IPropertyHandle> RowNameProperty;

	float PreviewLevel;
	float MaxPreviewLevel;
};
