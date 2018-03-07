// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTools/MediaTextureActions.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Materials/Material.h"
#include "Factories/MaterialFactoryNew.h"
#include "MediaTexture.h"
#include "Interfaces/ITextureEditorModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FAssetTypeActions_Base overrides
 *****************************************************************************/

bool FMediaTextureActions::CanFilter()
{
	return true;
}


void FMediaTextureActions::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	auto Textures = GetTypedWeakObjectPtrs<UTexture>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaTexture_CreateMaterial", "Create Material"),
		LOCTEXT("MediaTexture_CreateMaterialTooltip", "Creates a new material using this texture."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ClassIcon.Material" ),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMediaTextureActions::ExecuteCreateMaterial, Textures),
			FCanExecuteAction()
		)
	);

/*	MenuBuilder.AddMenuEntry(
		LOCTEXT("MediaTexture_CreateSlateBrush", "Create Slate Brush"),
		LOCTEXT("MediaTexture_CreateSlateBrushToolTip", "Creates a new slate brush using this texture."),
		FSlateIcon( FEditorStyle::GetStyleSetName(), "ClassIcon.SlateBrushAsset" ),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_MediaTexture::ExecuteCreateSlateBrush, Textures ),
			FCanExecuteAction()
		)
	);*/

	/* @todo AssetTypeActions Implement FindMaterials using the asset registry.
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
	}*/
}


uint32 FMediaTextureActions::GetCategories()
{
	return EAssetTypeCategories::MaterialsAndTextures;
}


FText FMediaTextureActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaTexture", "Media Texture");
}


UClass* FMediaTextureActions::GetSupportedClass() const
{
	return UMediaTexture::StaticClass();
}


FColor FMediaTextureActions::GetTypeColor() const
{
	return FColor::Red;
}


bool FMediaTextureActions::HasActions ( const TArray<UObject*>& InObjects ) const
{
	return true;
}


void FMediaTextureActions::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Texture = Cast<UTexture>(*ObjIt);

		if (Texture != nullptr)
		{
			ITextureEditorModule* TextureEditorModule = &FModuleManager::LoadModuleChecked<ITextureEditorModule>("TextureEditor");
			TextureEditorModule->CreateTextureEditor(Mode, EditWithinLevelEditor, Texture);
		}
	}
}


/* FAssetTypeActions_MediaTexture callbacks
 *****************************************************************************/

void FMediaTextureActions::ExecuteCreateMaterial( TArray<TWeakObjectPtr<UTexture>> Objects )
{
	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	const FString DefaultSuffix = TEXT("_Mat");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object != nullptr)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
			Factory->InitialTexture = Object;

			ContentBrowserSingleton.CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UMaterial::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();

			if (Object != nullptr)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
				Factory->InitialTexture = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterial::StaticClass(), Factory);

				if (NewAsset != nullptr)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			ContentBrowserSingleton.SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

/*
void FAssetTypeActions_MediaTexture::ExecuteCreateSlateBrush( TArray<TWeakObjectPtr<UTexture>> Objects )
{
	const FString DefaultSuffix = TEXT("_Brush");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object != nullptr)
		{
			// Determine the asset name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USlateBrushAssetFactory* Factory = ConstructObject<USlateBrushAssetFactory>(USlateBrushAssetFactory::StaticClass());
			Factory->InitialTexture = CastChecked<UTexture2D>(Object);
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USlateBrushAsset::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();

			if (Object != nullptr)
			{
				// Determine the asset name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USlateBrushAssetFactory* Factory = ConstructObject<USlateBrushAssetFactory>(USlateBrushAssetFactory::StaticClass());
				Factory->InitialTexture = CastChecked<UTexture2D>(Object);

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USlateBrushAsset::StaticClass(), Factory);

				if( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}*/


void FMediaTextureActions::ExecuteFindMaterials( TWeakObjectPtr<UTexture> Object )
{
	auto Texture = Object.Get();

	if (Texture != nullptr)
	{
		// @todo AssetTypeActions Implement FindMaterials using the asset registry.
	}
}


#undef LOCTEXT_NAMESPACE
