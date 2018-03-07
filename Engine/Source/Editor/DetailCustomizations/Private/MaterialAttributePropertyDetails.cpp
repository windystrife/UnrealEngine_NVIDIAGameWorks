// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialAttributePropertyDetails.h"
#include "Widgets/Text/STextBlock.h"
#include "MaterialShared.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SComboBox.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"

TSharedRef<IDetailCustomization> FMaterialAttributePropertyDetails::MakeInstance()
{
	return MakeShareable(new FMaterialAttributePropertyDetails);
}

void FMaterialAttributePropertyDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Populate combo boxes with material property list
	FMaterialAttributeDefinitionMap::GetDisplayNameToIDList(AttributeNameToIDList);

	AttributeDisplayNameList.Empty(AttributeNameToIDList.Num());
	for (const TPair<FString, FGuid>& NameGUIDPair : AttributeNameToIDList)
	{
		AttributeDisplayNameList.Add(MakeShareable(new FString(NameGUIDPair.Key)));
	}

	// Fetch root property we're dealing with
	TSharedPtr<IPropertyHandle> PropertyGetArray = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMaterialExpressionGetMaterialAttributes, AttributeGetTypes));
	TSharedPtr<IPropertyHandle> PropertySetArray = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UMaterialExpressionSetMaterialAttributes, AttributeSetTypes));
	TSharedPtr<IPropertyHandle> PropertyArray;

	if (PropertyGetArray->IsValidHandle())
	{
		PropertyArray = PropertyGetArray;
	}
	else if (PropertySetArray->IsValidHandle())
	{
		PropertyArray = PropertySetArray;
	}
	
	check(PropertyArray->IsValidHandle());

	// Add builder for children to handle array changes
	TSharedRef<FDetailArrayBuilder> ArrayChildBuilder = MakeShareable(new FDetailArrayBuilder(PropertyArray.ToSharedRef()));
	ArrayChildBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FMaterialAttributePropertyDetails::OnBuildChild));

	IDetailCategoryBuilder& AttributesCategory = DetailLayout.EditCategory("MaterialAttributes", FText::GetEmpty(), ECategoryPriority::Important);
	AttributesCategory.AddCustomBuilder(ArrayChildBuilder);
}

void FMaterialAttributePropertyDetails::OnBuildChild(TSharedRef<IPropertyHandle> ChildHandle, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	// Add an overridden combo box
	IDetailPropertyRow& PropertyArrayRow = ChildrenBuilder.AddProperty(ChildHandle);

	PropertyArrayRow.CustomWidget()
	.NameContent()
	[
		ChildHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&AttributeDisplayNameList)
 			.OnGenerateWidget_Lambda( [] (TSharedPtr<FString> InItem)
			{
				return SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::FromString(*InItem));
			})
			.OnSelectionChanged_Lambda( [=] (TSharedPtr<FString> Selection, ESelectInfo::Type)
			{
				if (ChildHandle->IsValidHandle())
				{
					// Convert display name to attribute ID
					for (const auto& NameIDPair : AttributeNameToIDList)
					{
						if (NameIDPair.Key == *Selection)
						{
							ChildHandle->SetValueFromFormattedString(NameIDPair.Value.ToString(EGuidFormats::Digits));
							break;
						}
					}
				}
			})
			.ContentPadding(FMargin(2, 0))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text_Lambda( [=]() -> FText
				{ 
					if (ChildHandle->IsValidHandle())
					{
						// Convert attribute ID string to display name
						FString IDString; FGuid IDValue;
						ChildHandle->GetValueAsFormattedString(IDString);
						FGuid::ParseExact(IDString, EGuidFormats::Digits, IDValue);

						FString AttributeName = FMaterialAttributeDefinitionMap::GetDisplayName(IDValue);
						return FText::FromString(AttributeName);
					}

					return FText::GetEmpty();
				} )
			]
		]
	];
}

