// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SoftClassPathCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "EditorClassUtils.h"

void FSoftClassPathCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = InPropertyHandle;

	const FString& MetaClassName = PropertyHandle->GetMetaData("MetaClass");
	const FString& RequiredInterfaceName = PropertyHandle->GetMetaData("RequiredInterface"); // This was the old name, switch to MustImplement to synchronize with class property
	const FString& MustImplementName = PropertyHandle->GetMetaData("MustImplement"); 
	const bool bAllowAbstract = PropertyHandle->HasMetaData("AllowAbstract");
	const bool bIsBlueprintBaseOnly = PropertyHandle->HasMetaData("IsBlueprintBaseOnly") || PropertyHandle->HasMetaData("BlueprintBaseOnly");
	const bool bAllowNone = !(PropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	const bool bShowTreeView = PropertyHandle->HasMetaData("ShowTreeView");
	const bool bHideViewOptions = PropertyHandle->HasMetaData("HideViewOptions");
	
	const UClass* const MetaClass = !MetaClassName.IsEmpty()
		? FEditorClassUtils::GetClassFromString(MetaClassName)
		: UClass::StaticClass();
	const UClass* const RequiredInterface = !RequiredInterfaceName.IsEmpty()
		? FEditorClassUtils::GetClassFromString(RequiredInterfaceName)
		: FEditorClassUtils::GetClassFromString(MustImplementName);

	HeaderRow
	.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		// Add a class entry box.  Even though this isn't an class entry, we will simulate one
		SNew(SClassPropertyEntryBox)
			.MetaClass(MetaClass)
			.RequiredInterface(RequiredInterface)
			.AllowAbstract(bAllowAbstract)
			.IsBlueprintBaseOnly(bIsBlueprintBaseOnly)
			.AllowNone(bAllowNone)
			.ShowTreeView(bShowTreeView)
			.HideViewOptions(bHideViewOptions)
			.SelectedClass(this, &FSoftClassPathCustomization::OnGetClass)
			.OnSetClass(this, &FSoftClassPathCustomization::OnSetClass)
	];
}

void FSoftClassPathCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

const UClass* FSoftClassPathCustomization::OnGetClass() const
{
	FString ClassName;
	PropertyHandle->GetValueAsFormattedString(ClassName);

	// Do we have a valid cached class pointer?
	const UClass* Class = CachedClassPtr.Get();
	if(!Class || Class->GetPathName() != ClassName)
	{
		Class = FEditorClassUtils::GetClassFromString(ClassName);
		CachedClassPtr = Class;
	}
	return Class;
}

void FSoftClassPathCustomization::OnSetClass(const UClass* NewClass)
{
	if (PropertyHandle->SetValueFromFormattedString((NewClass) ? NewClass->GetPathName() : "None") == FPropertyAccess::Result::Success)
	{
		CachedClassPtr = NewClass;
	}
}
