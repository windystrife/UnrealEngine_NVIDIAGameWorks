// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "UObject/StructOnScope.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilderImpl.h"

class IDetailGroup;

class FCustomChildrenBuilder : public IDetailChildrenBuilder
{
public:
	FCustomChildrenBuilder(TSharedRef<FDetailCategoryImpl> InParentCategory, TSharedPtr<IDetailGroup> InParentGroup = nullptr)
		: ParentCategory(InParentCategory)
		, ParentGroup(InParentGroup)
	{}

	virtual IDetailChildrenBuilder& AddCustomBuilder(TSharedRef<class IDetailCustomNodeBuilder> InCustomBuilder) override;
	virtual IDetailGroup& AddGroup(FName GroupName, const FText& LocalizedDisplayName) override;
	virtual FDetailWidgetRow& AddCustomRow(const FText& SearchString) override;
	virtual IDetailPropertyRow& AddProperty(TSharedRef<IPropertyHandle> PropertyHandle) override;
	virtual IDetailPropertyRow* AddExternalObjects(const TArray<UObject*>& Objects, FName UniqueIdName = NAME_None) override;
	virtual IDetailPropertyRow* AddExternalObjectProperty(const TArray<UObject*>& Objects, FName PropertyName, FName UniqueIdName = NAME_None) override;
	virtual IDetailPropertyRow* AddExternalStructure(TSharedRef<FStructOnScope> ChildStructure, FName UniqueIdName = NAME_None) override;
	virtual IDetailPropertyRow* AddExternalStructureProperty(TSharedRef<FStructOnScope> ChildStructure, FName PropertyName, FName UniqueIdName = NAME_None) override;
	virtual TArray<TSharedPtr<IPropertyHandle>> AddAllExternalStructureProperties(TSharedRef<FStructOnScope> ChildStructure) override;
	virtual TSharedRef<SWidget> GenerateStructValueWidget(TSharedRef<IPropertyHandle> StructPropertyHandle) override;
	virtual IDetailCategoryBuilder& GetParentCategory() const override;
	virtual IDetailGroup* GetParentGroup() const override;

	const TArray< FDetailLayoutCustomization >& GetChildCustomizations() const { return ChildCustomizations; }

	/** Set the user customized reset to default for the children of this builder */
	FCustomChildrenBuilder& OverrideResetChildrenToDefault(const FResetToDefaultOverride& ResetToDefault);

private:
	TArray< FDetailLayoutCustomization > ChildCustomizations;
	TWeakPtr<FDetailCategoryImpl> ParentCategory;
	TWeakPtr<IDetailGroup> ParentGroup;

	/** User customized reset to default on children */
	TOptional<FResetToDefaultOverride> CustomResetChildToDefault;
};
