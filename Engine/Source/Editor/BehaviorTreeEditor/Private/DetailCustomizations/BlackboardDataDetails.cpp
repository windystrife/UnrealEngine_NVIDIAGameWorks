// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizations/BlackboardDataDetails.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "BehaviorTree/BlackboardData.h"

#define LOCTEXT_NAMESPACE "BlackboardDataDetails"

TSharedRef<IDetailCustomization> FBlackboardDataDetails::MakeInstance(FOnGetSelectedBlackboardItemIndex InOnGetSelectedBlackboardItemIndex)
{
	return MakeShareable( new FBlackboardDataDetails(InOnGetSelectedBlackboardItemIndex) );
}

void FBlackboardDataDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	// First hide all keys
	DetailLayout.HideProperty(TEXT("Keys"));
	DetailLayout.HideProperty(TEXT("ParentKeys"));

	// Now show only the currently selected key
	bool bIsInherited = false;
	int32 CurrentSelection = INDEX_NONE;
	if(OnGetSelectedBlackboardItemIndex.IsBound())
	{
		CurrentSelection = OnGetSelectedBlackboardItemIndex.Execute(bIsInherited);
	}

	if(CurrentSelection >= 0)
	{
		TSharedPtr<IPropertyHandle> KeysHandle = bIsInherited ? DetailLayout.GetProperty(TEXT("ParentKeys")) : DetailLayout.GetProperty(TEXT("Keys"));
		check(KeysHandle.IsValid());
		uint32 NumChildKeys = 0;
		KeysHandle->GetNumChildren(NumChildKeys);
		if((uint32)CurrentSelection < NumChildKeys)
		{
			TSharedPtr<IPropertyHandle> KeyHandle = KeysHandle->GetChildHandle((uint32)CurrentSelection);

			IDetailCategoryBuilder& DetailCategoryBuilder = DetailLayout.EditCategory("Key");
			TSharedPtr<IPropertyHandle> EntryNameProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, EntryName));
			DetailCategoryBuilder.AddCustomRow(LOCTEXT("EntryNameLabel", "Entry Name"))
			.NameContent()
			[
				EntryNameProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				.IsEnabled(true)
				+SHorizontalBox::Slot()
				[
					EntryNameProperty->CreatePropertyValueWidget()
				]
			];

#if WITH_EDITORONLY_DATA
// 			TSharedPtr<IPropertyHandle> EntryDescriptionHandle = ElementProperty->GetChildHandle("EntryDescription");
			TSharedPtr<IPropertyHandle> EntryDescriptionHandle = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, EntryDescription));

			DetailCategoryBuilder.AddProperty(EntryDescriptionHandle);
#endif

			TSharedPtr<IPropertyHandle> KeyTypeProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, KeyType));
			DetailCategoryBuilder.AddProperty(KeyTypeProperty);

			TSharedPtr<IPropertyHandle> bInstanceSyncedProperty = KeyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlackboardEntry, bInstanceSynced));
			DetailCategoryBuilder.AddProperty(bInstanceSyncedProperty);
		}	
	}
}

#undef LOCTEXT_NAMESPACE
