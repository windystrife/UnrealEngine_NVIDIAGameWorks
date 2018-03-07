// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Engine/CurveTable.h"
#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"

/**
 * Customizes a DataTable asset to use a dropdown
 */
class DETAILCUSTOMIZATIONS_API FCurveTableCustomizationLayout : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance() 
	{
		return MakeShareable( new FCurveTableCustomizationLayout );
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override
	{
		this->StructPropertyHandle = InStructPropertyHandle;

		HeaderRow
			.NameContent()
			[
				InStructPropertyHandle->CreatePropertyNameWidget( FText::GetEmpty(), FText::GetEmpty(), false )
			];
	}

	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override
	{
		/** Get all the existing property handles */
		CurveTablePropertyHandle = InStructPropertyHandle->GetChildHandle( "CurveTable" );
		RowNamePropertyHandle = InStructPropertyHandle->GetChildHandle( "RowName" );

		if( CurveTablePropertyHandle->IsValidHandle() && RowNamePropertyHandle->IsValidHandle() )
		{
			/** Queue up a refresh of the selected item, not safe to do from here */
			StructCustomizationUtils.GetPropertyUtilities()->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FCurveTableCustomizationLayout::OnCurveTableChanged));

			CreateCurveTableChildProperty(StructBuilder);

			FSimpleDelegate OnCurveTableChangedDelegate = FSimpleDelegate::CreateSP( this, &FCurveTableCustomizationLayout::OnCurveTableChanged );
			CurveTablePropertyHandle->SetOnPropertyValueChanged( OnCurveTableChangedDelegate );

			/** Construct a combo box widget to select from a list of valid options */
			StructBuilder.AddCustomRow( NSLOCTEXT("CurveTable", "RowNameLabel", "Row Name" ) )
			.NameContent()
				[
					SNew( STextBlock )
					.Text( NSLOCTEXT("CurveTable", "RowNameLabel", "Row Name" ) )
					.Font( StructCustomizationUtils.GetRegularFont() )
				]
			.ValueContent()
				[
					SAssignNew( RowNameComboButton, SComboButton )
					.OnGetMenuContent( this, &FCurveTableCustomizationLayout::GetListContent )
					.ContentPadding( FMargin( 2.0f, 2.0f ) )
					.ButtonContent()
					[
						SNew( STextBlock )
						.Text( this, &FCurveTableCustomizationLayout::GetRowNameComboBoxContentText )
						.ToolTipText( this, &FCurveTableCustomizationLayout::GetRowNameComboBoxContentText )
					]
				];
		}
	}
	
	virtual void CreateCurveTableChildProperty(IDetailChildrenBuilder& StructBuilder)
	{
		/** Edit the data table uobject as normal */
		StructBuilder.AddProperty( CurveTablePropertyHandle.ToSharedRef() );
	}

protected:
	/** Init the contents the combobox sources its data off */
	TSharedPtr<FString> InitWidgetContent()
	{
		TSharedPtr<FString> InitialValue = MakeShareable( new FString( TEXT( "None" ) ) );
		
		FName RowName;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowName );
		RowNames.Empty();

		if ( RowResult != FPropertyAccess::Fail )
		{
			/** Get the properties we wish to work with */
			UCurveTable* CurveTable = NULL;
			CurveTablePropertyHandle->GetValue((UObject*&)CurveTable);

			if ( CurveTable != NULL )
			{
				/** Extract all the row names from the RowMap */
				for ( TMap<FName, FRichCurve*>::TConstIterator Iterator(CurveTable->RowMap); Iterator; ++Iterator )
				{
					/** Create a simple array of the row names */
					TSharedRef<FString> RowNameItem = MakeShareable(new FString(Iterator.Key().ToString()));
					RowNames.Add(RowNameItem);

					/** Set the initial value to the currently selected item */
					if ( Iterator.Key() == RowName )
					{
						InitialValue = RowNameItem;
					}
				}
			}

			/** Reset the initial value to ensure a valid entry is set */
			if ( RowResult != FPropertyAccess::MultipleValues )
			{
				TArray<void*> RawData;

				// This raw data access is necessary to avoid setting the value during details customization setup which would cause infinite recursion repetedly reinitializing this customization
				RowNamePropertyHandle->AccessRawData(RawData);
				if ( RawData.Num() == 1 )
				{
					FName* RawFName = (FName*)RawData[0];
					*RawFName = **InitialValue;
				}
			}
		}

		return InitialValue;
	}

	/** Returns the ListView for the ComboButton */
	TSharedRef<SWidget> GetListContent();

	/** Delegate to refresh the drop down when the datatable changes */
	void OnCurveTableChanged()
	{
		CurrentSelectedItem = InitWidgetContent();
		if( RowNameComboListView.IsValid() )
		{
			RowNameComboListView->SetSelection( CurrentSelectedItem );
			RowNameComboListView->RequestListRefresh();
		}
	}

	/** Return the representation of the the row names to display */
	TSharedRef<ITableRow>  HandleRowNameComboBoxGenarateWidget( TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable )
	{
		return
			SNew( STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew( STextBlock ).Text( FText::FromString(*InItem) )
			];
	}

	/** Display the current selection */
	FText GetRowNameComboBoxContentText( ) const
	{
		FName RowNameValue;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowNameValue );
		if ( RowResult != FPropertyAccess::MultipleValues )
		{
			TSharedPtr<FString> SelectedRowName = CurrentSelectedItem;
			if ( SelectedRowName.IsValid() )
			{
				return FText::FromString(*SelectedRowName);
			}
			else
			{
				return NSLOCTEXT( "CurveTableCustomization", "None", "None" );
			}
		}
		return NSLOCTEXT( "CurveTableCustomization", "MultipleValues", "Multiple Values" );
	}

	/** Update the root data on a change of selection */
	void OnSelectionChanged( TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo )
	{
		if( SelectedItem.IsValid() )
		{
			CurrentSelectedItem = SelectedItem; 
			FName NewValue = FName( **SelectedItem );
			RowNamePropertyHandle->SetValue( NewValue );

			// Close the combo
			RowNameComboButton->SetIsOpen( false );
		}
	}

	/** Called by Slate when the filter box changes text. */
	void OnFilterTextChanged( const FText& InFilterText )
	{
		FString CurrentFilterText = InFilterText.ToString();

		FName RowName;
		const FPropertyAccess::Result RowResult = RowNamePropertyHandle->GetValue( RowName );
		RowNames.Empty();

		/** Get the properties we wish to work with */
		UCurveTable* CurveTable = NULL;
		CurveTablePropertyHandle->GetValue( ( UObject*& )CurveTable );

		if( CurveTable != NULL )
		{
			/** Extract all the row names from the RowMap */
			for( TMap<FName, FRichCurve*>::TConstIterator Iterator( CurveTable->RowMap ); Iterator; ++Iterator )
			{
				/** Create a simple array of the row names */
				FString RowString = Iterator.Key().ToString();
				if( CurrentFilterText == TEXT("") || RowString.Contains(CurrentFilterText) )
				{
					TSharedRef<FString> RowNameItem = MakeShareable( new FString( RowString ) );				
					RowNames.Add( RowNameItem );					
				}			
			}
		}		
		RowNameComboListView->RequestListRefresh();
	}

	/** The comboButton objects */
	TSharedPtr<SComboButton> RowNameComboButton;
	TSharedPtr<SListView<TSharedPtr<FString> > > RowNameComboListView;
	TSharedPtr<FString> CurrentSelectedItem;	
	/** Handle to the struct properties being customized */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<IPropertyHandle> CurveTablePropertyHandle;
	TSharedPtr<IPropertyHandle> RowNamePropertyHandle;
	/** A cached copy of strings to populate the combo box */
	TArray<TSharedPtr<FString> > RowNames;
};
