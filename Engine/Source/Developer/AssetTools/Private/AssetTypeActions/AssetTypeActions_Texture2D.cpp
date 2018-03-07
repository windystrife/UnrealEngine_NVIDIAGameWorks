// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Texture2D.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "EditorStyleSet.h"
#include "Factories/SlateBrushAssetFactory.h"
#include "Slate/SlateBrushAsset.h"
#include "AssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Texture2D::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Texture::GetActions(InObjects, MenuBuilder);

	auto Textures = GetTypedWeakObjectPtrs<UTexture>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Texture2D_CreateSlateBrush", "Create Slate Brush"),
		LOCTEXT("Texture2D_CreateSlateBrushToolTip", "Creates a new slate brush using this texture."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.SlateBrushAsset"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture2D::ExecuteCreateSlateBrush, Textures ),
			FCanExecuteAction()
			)
		);
}

void FAssetTypeActions_Texture2D::ExecuteCreateSlateBrush(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	const FString DefaultSuffix = TEXT("_Brush");

	if( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if( Object )
		{
			// Determine the asset name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USlateBrushAssetFactory* Factory = NewObject<USlateBrushAssetFactory>();
			Factory->InitialTexture = CastChecked<UTexture2D>(Object);
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USlateBrushAsset::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for( auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt )
		{
			auto Object = (*ObjIt).Get();
			if( Object )
			{
				// Determine the asset name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USlateBrushAssetFactory* Factory = NewObject<USlateBrushAssetFactory>();
				Factory->InitialTexture = CastChecked<UTexture2D>(Object);

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USlateBrushAsset::StaticClass(), Factory);

				if( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE
