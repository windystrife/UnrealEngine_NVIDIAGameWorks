// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SlateWidgetStyle.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


void FAssetTypeActions_SlateWidgetStyle::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Styles = GetTypedWeakObjectPtrs< USlateWidgetStyleAsset >(InObjects);
}

void FAssetTypeActions_SlateWidgetStyle::OpenAssetEditor( const TArray<UObject*>& Objects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	struct Local
	{
		static TArray<UObject*> GetSubObjects(const TArray<UObject*>& InObjects)
		{
			TArray<UObject*> SubObjects;
			for(UObject* Object : InObjects)
			{
				auto Style = Cast<USlateWidgetStyleAsset>(Object);
				if(Style && Style->CustomStyle)
				{
					SubObjects.Add(Style->CustomStyle);
				}
			}
			return SubObjects;
		}
	};

	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Objects, FSimpleAssetEditor::FGetDetailsViewObjects::CreateStatic(&Local::GetSubObjects));
}

#undef LOCTEXT_NAMESPACE
