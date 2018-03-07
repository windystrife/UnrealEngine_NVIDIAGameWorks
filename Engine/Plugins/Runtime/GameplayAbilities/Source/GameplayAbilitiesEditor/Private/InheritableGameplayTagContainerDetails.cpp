// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InheritableGameplayTagContainerDetails.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "InheritableGameplayTagContainerDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FInheritableGameplayTagContainerDetails::MakeInstance()
{
	return MakeShareable(new FInheritableGameplayTagContainerDetails());
}

void FInheritableGameplayTagContainerDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FInheritableGameplayTagContainerDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	CombinedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("CombinedTags"));
	AddedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Added"));
	RemovedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Removed"));

	FSimpleDelegate OnTagValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FInheritableGameplayTagContainerDetails::OnTagsChanged);
	AddedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);
	RemovedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);

	StructBuilder.AddProperty(CombinedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddProperty(AddedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddProperty(RemovedTagContainerPropertyHandle.ToSharedRef());
}

void FInheritableGameplayTagContainerDetails::OnTagsChanged()
{
// 	CombinedTagContainerPropertyHandle->NotifyPreChange();
// 
// 	TSharedPtr<IPropertyHandle> StructPropertyHandle = CombinedTagContainerPropertyHandle->GetParentHandle();
// 	
// 	TArray<void*> RawData;
// 	StructPropertyHandle->AccessRawData(RawData);
// 
// 	for (void* RawPtr : RawData)
// 	{
// 		FInheritedTagContainer* GameplayTagContainer = reinterpret_cast<FInheritedTagContainer*>(RawPtr);
// 		if (GameplayTagContainer)
// 		{
// 			GameplayTagContainer->UpdateInheritedTagProperties();
// 		}
// 	}
// 
// 	CombinedTagContainerPropertyHandle->NotifyPostChange();
}

#undef LOCTEXT_NAMESPACE
