// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Enum.h"
#include "BlueprintEditorModule.h"
#include "AssetData.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_Enum::GetAssetDescription(const FAssetData& AssetData) const
{
	return AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UUserDefinedEnum, EnumDescription));
}

void FAssetTypeActions_Enum::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UUserDefinedEnum* UDEnum = Cast<UUserDefinedEnum>(*ObjIt))
		{
			BlueprintEditorModule.CreateUserDefinedEnumEditor(Mode, EditWithinLevelEditor, UDEnum);
		}
	}
}

#undef LOCTEXT_NAMESPACE
