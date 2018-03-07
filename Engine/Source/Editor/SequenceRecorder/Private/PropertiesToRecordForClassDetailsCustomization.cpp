// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PropertiesToRecordForClassDetailsCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Engine/GameViewportClient.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "SClassPropertyRecorderSettings.h"
#include "SequenceRecorderSettings.h"

void FPropertiesToRecordForClassDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.
	NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPropertiesToRecordForClassDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedRef<IPropertyHandle> ClassProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPropertiesToRecordForClass, Class)).ToSharedRef();
	TSharedRef<IPropertyHandle> PropertiesProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPropertiesToRecordForClass, Properties)).ToSharedRef();

	ChildBuilder.AddProperty(ClassProperty);

	ChildBuilder.AddProperty(PropertiesProperty)
	.CustomWidget()
	.NameContent()
	[
		PropertiesProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(300.0f)
	.MaxDesiredWidth(400.0f)
	[
		SNew(SClassPropertyRecorderSettings, ClassProperty, PropertiesProperty, CustomizationUtils)
	];
}
