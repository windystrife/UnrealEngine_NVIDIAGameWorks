// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "EditorStyleSet.h"
#include "Materials/Material.h"
#include "Factories/MaterialFactoryNew.h"
#include "EditorFramework/AssetImportData.h"
#include "AssetTools.h"
#include "Interfaces/ITextureEditorModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Factories/SubUVAnimationFactory.h"
#include "Particles/SubUVAnimation.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Texture::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	auto Textures = GetTypedWeakObjectPtrs<UTexture>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Texture_CreateMaterial", "Create Material"),
		LOCTEXT("Texture_CreateMaterialTooltip", "Creates a new material using this texture."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.Material"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture::ExecuteCreateMaterial, Textures ),
			FCanExecuteAction()
			)
		);

	// @todo AssetTypeActions Implement FindMaterials using the asset registry.
	/*
	if ( InObjects.Num() == 1 )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Texture_FindMaterials", "Find Materials Using This"),
			LOCTEXT("Texture_FindMaterialsTooltip", "Finds all materials that use this material in the content browser."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetTypeActions_Texture::ExecuteFindMaterials, Textures(0) ),
				FCanExecuteAction()
				)
			);
	}
	*/
}

void FAssetTypeActions_Texture::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		UTexture* Texture = CastChecked<UTexture>(Asset);
		Texture->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_Texture::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Texture = Cast<UTexture>(*ObjIt);
		if (Texture != NULL)
		{
			ITextureEditorModule* TextureEditorModule = &FModuleManager::LoadModuleChecked<ITextureEditorModule>("TextureEditor");
			TextureEditorModule->CreateTextureEditor(Mode, EditWithinLevelEditor, Texture);
		}
	}
}

void FAssetTypeActions_Texture::ExecuteCreateMaterial(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	const FString DefaultSuffix = TEXT("_Mat");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			Factory->InitialTexture = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UMaterial::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterial::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_Texture::ExecuteCreateSubUVAnimation(TArray<TWeakObjectPtr<UTexture>> Objects)
{
	const FString DefaultSuffix = TEXT("_SubUV");

	if ( Objects.Num() == 1 )
	{
		UTexture2D* Object = Cast<UTexture2D>(Objects[0].Get());

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USubUVAnimationFactory* Factory = NewObject<USubUVAnimationFactory>();
			Factory->InitialTexture = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USubUVAnimation::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			UTexture2D* Object = Cast<UTexture2D>((*ObjIt).Get());

			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USubUVAnimationFactory* Factory = NewObject<USubUVAnimationFactory>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USubUVAnimation::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_Texture::ExecuteFindMaterials(TWeakObjectPtr<UTexture> Object)
{
	auto Texture = Object.Get();
	if ( Texture )
	{
		// @todo AssetTypeActions Implement FindMaterials using the asset registry.
	}
}

#undef LOCTEXT_NAMESPACE
