// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CustomChildBuilder.h"
#include "Modules/ModuleManager.h"
#include "DetailGroup.h"
#include "PropertyHandleImpl.h"
#include "DetailPropertyRow.h"
#include "ObjectPropertyNode.h"

IDetailChildrenBuilder& FCustomChildrenBuilder::AddCustomBuilder( TSharedRef<class IDetailCustomNodeBuilder> InCustomBuilder )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.CustomBuilderRow = MakeShareable( new FDetailCustomBuilderRow( InCustomBuilder ) );

	ChildCustomizations.Add( NewCustomization );
	return *this;
}

IDetailGroup& FCustomChildrenBuilder::AddGroup( FName GroupName, const FText& LocalizedDisplayName )
{
	FDetailLayoutCustomization NewCustomization;
	NewCustomization.DetailGroup = MakeShareable( new FDetailGroup( GroupName, ParentCategory.Pin().ToSharedRef(), LocalizedDisplayName ) );

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.DetailGroup;
}

FDetailWidgetRow& FCustomChildrenBuilder::AddCustomRow( const FText& SearchString )
{
	TSharedRef<FDetailWidgetRow> NewRow = MakeShareable( new FDetailWidgetRow );
	FDetailLayoutCustomization NewCustomization;

	NewRow->FilterString( SearchString );

	NewCustomization.WidgetDecl = NewRow;

	ChildCustomizations.Add( NewCustomization );
	return *NewRow;
}

IDetailPropertyRow& FCustomChildrenBuilder::AddProperty( TSharedRef<IPropertyHandle> PropertyHandle )
{
	check( PropertyHandle->IsValidHandle() )

	FDetailLayoutCustomization NewCustomization;
	NewCustomization.PropertyRow = MakeShareable( new FDetailPropertyRow( StaticCastSharedRef<FPropertyHandleBase>( PropertyHandle )->GetPropertyNode(), ParentCategory.Pin().ToSharedRef() ) );

	if (CustomResetChildToDefault.IsSet())
	{
		NewCustomization.PropertyRow->OverrideResetToDefault(CustomResetChildToDefault.GetValue());
	}

	ChildCustomizations.Add( NewCustomization );

	return *NewCustomization.PropertyRow;
}


IDetailPropertyRow* FCustomChildrenBuilder::AddExternalStructure(TSharedRef<FStructOnScope> ChildStructure, FName UniqueIdName)
{
	return AddExternalStructureProperty(ChildStructure, NAME_None, UniqueIdName);
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalStructureProperty(TSharedRef<FStructOnScope> ChildStructure, FName PropertyName, FName UniqueIdName)
{
	FDetailLayoutCustomization NewCustomization;

	TSharedRef<FDetailCategoryImpl> ParentCategoryRef = ParentCategory.Pin().ToSharedRef();

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(ChildStructure, PropertyName, ParentCategoryRef, NewCustomization);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	NewRow->SetCustomExpansionId(UniqueIdName);

	if (NewRow.IsValid())
	{
		TSharedPtr<FPropertyNode> PropertyNode = NewRow->GetPropertyNode();
		TSharedPtr<FComplexPropertyNode> RootNode = StaticCastSharedRef<FComplexPropertyNode>(PropertyNode->FindComplexParent()->AsShared());

		ChildCustomizations.Add(NewCustomization);
	}

	return NewRow.Get();
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalObjects(const TArray<UObject *>& Objects, FName UniqueIdName)
{
	return AddExternalObjectProperty(Objects, NAME_None, UniqueIdName);
}

TArray<TSharedPtr<IPropertyHandle>> FCustomChildrenBuilder::AddAllExternalStructureProperties(TSharedRef<FStructOnScope> ChildStructure)
{
	return ParentCategory.Pin()->AddAllExternalStructureProperties(ChildStructure);
}

IDetailPropertyRow* FCustomChildrenBuilder::AddExternalObjectProperty(const TArray<UObject*>& Objects, FName PropertyName, FName UniqueIdName)
{
	FDetailLayoutCustomization NewCustomization;

	TSharedRef<FDetailCategoryImpl> ParentCategoryRef = ParentCategory.Pin().ToSharedRef();

	FDetailPropertyRow::MakeExternalPropertyRowCustomization(Objects, PropertyName, ParentCategoryRef, NewCustomization);

	TSharedPtr<FDetailPropertyRow> NewRow = NewCustomization.PropertyRow;

	NewRow->SetCustomExpansionId(UniqueIdName);

	if (NewRow.IsValid())
	{
		TSharedPtr<FPropertyNode> PropertyNode = NewRow->GetPropertyNode();
		TSharedPtr<FObjectPropertyNode> RootNode = StaticCastSharedRef<FObjectPropertyNode>(PropertyNode->FindObjectItemParent()->AsShared());

		ChildCustomizations.Add(NewCustomization);
	}

	return NewRow.Get();
}

class SStandaloneCustomStructValue : public SCompoundWidget, public IPropertyTypeCustomizationUtils
{
public:
	SLATE_BEGIN_ARGS( SStandaloneCustomStructValue )
	{}
	SLATE_END_ARGS()
	
	void Construct( const FArguments& InArgs, TSharedPtr<IPropertyTypeCustomization> InCustomizationInterface, TSharedRef<IPropertyHandle> InStructPropertyHandle, TSharedRef<FDetailCategoryImpl> InParentCategory )
	{
		CustomizationInterface = InCustomizationInterface;
		StructPropertyHandle = InStructPropertyHandle;
		ParentCategory = InParentCategory;
		CustomPropertyWidget = MakeShareable(new FDetailWidgetRow);

		CustomizationInterface->CustomizeHeader(InStructPropertyHandle, *CustomPropertyWidget, *this);

		ChildSlot
		[
			CustomPropertyWidget->ValueWidget.Widget
		];
	}

	virtual TSharedPtr<FAssetThumbnailPool> GetThumbnailPool() const override
	{
		TSharedPtr<FDetailCategoryImpl> ParentCategoryPinned = ParentCategory.Pin();
		return ParentCategoryPinned.IsValid() ? ParentCategoryPinned->GetParentLayout().GetThumbnailPool() : NULL;
	}

private:
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	TSharedPtr<IPropertyTypeCustomization> CustomizationInterface;
	TSharedPtr<IPropertyHandle> StructPropertyHandle;
	TSharedPtr<FDetailWidgetRow> CustomPropertyWidget;
};


TSharedRef<SWidget> FCustomChildrenBuilder::GenerateStructValueWidget( TSharedRef<IPropertyHandle> StructPropertyHandle )
{
	UStructProperty* StructProperty = CastChecked<UStructProperty>( StructPropertyHandle->GetProperty() );

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	IDetailsViewPrivate* DetailsView = ParentCategory.Pin()->GetDetailsView();

	FPropertyTypeLayoutCallback LayoutCallback = PropertyEditorModule.GetPropertyTypeCustomization(StructProperty, *StructPropertyHandle, DetailsView ? DetailsView->GetCustomPropertyTypeLayoutMap() : FCustomPropertyTypeLayoutMap() );
	if (LayoutCallback.IsValid())
	{
		TSharedRef<IPropertyTypeCustomization> CustomStructInterface = LayoutCallback.GetCustomizationInstance();

		return SNew( SStandaloneCustomStructValue, CustomStructInterface, StructPropertyHandle, ParentCategory.Pin().ToSharedRef() );
	}
	else
	{
		// Uncustomized structs have nothing for their value content
		return SNullWidget::NullWidget;
	}
}

IDetailCategoryBuilder& FCustomChildrenBuilder::GetParentCategory() const
{
	return *ParentCategory.Pin();
}

FCustomChildrenBuilder& FCustomChildrenBuilder::OverrideResetChildrenToDefault(const FResetToDefaultOverride& ResetToDefault)
{
	CustomResetChildToDefault = ResetToDefault;
	return *this;
}

IDetailGroup* FCustomChildrenBuilder::GetParentGroup() const
{
	return ParentGroup.IsValid() ? ParentGroup.Pin().Get() : nullptr;
}
