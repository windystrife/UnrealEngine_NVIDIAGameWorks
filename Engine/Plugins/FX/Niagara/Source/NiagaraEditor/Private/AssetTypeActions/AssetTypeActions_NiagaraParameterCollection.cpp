// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NiagaraParameterCollection.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraParameterCollectionToolkit.h"
#include "NiagaraParameterCollectionFactoryNew.h"
#include "NiagaraEditorStyle.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "MultiBoxBuilder.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_NiagaraParameterCollection"

FColor FAssetTypeActions_NiagaraParameterCollection::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ParameterCollection").ToFColor(true);
}

void FAssetTypeActions_NiagaraParameterCollection::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto NPC = Cast<UNiagaraParameterCollection>(*ObjIt);
		if (NPC != NULL)
		{
			TSharedRef< FNiagaraParameterCollectionToolkit > NewNiagaraNPCToolkit(new FNiagaraParameterCollectionToolkit());
			NewNiagaraNPCToolkit->Initialize(Mode, EditWithinLevelEditor, NPC);
		}
	}
}

void FAssetTypeActions_NiagaraParameterCollection::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Collections = GetTypedWeakObjectPtrs<UNiagaraParameterCollection>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewNPC", "Create Niagara Parameter Collection Instance"),
		LOCTEXT("NewNPCTooltip", "Creates an instance of this Niagara Parameter Collection."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.MaterialInstanceActor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_NiagaraParameterCollection::ExecuteNewNPC, Collections)
		)
	);
}

void FAssetTypeActions_NiagaraParameterCollection::ExecuteNewNPC(TArray<TWeakObjectPtr<UNiagaraParameterCollection>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Create an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			UNiagaraParameterCollectionInstanceFactoryNew* Factory = NewObject<UNiagaraParameterCollectionInstanceFactoryNew>();
			Factory->InitialParent = Object;

			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UNiagaraParameterCollectionInstance::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (Object)
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UNiagaraParameterCollectionInstanceFactoryNew* Factory = NewObject<UNiagaraParameterCollectionInstanceFactoryNew>();
				Factory->InitialParent = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UNiagaraParameterCollectionInstance::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

UClass* FAssetTypeActions_NiagaraParameterCollection::GetSupportedClass() const
{
	return UNiagaraParameterCollection::StaticClass();
}

//////////////////////////////////////////////////////////////////////////

FColor FAssetTypeActions_NiagaraParameterCollectionInstance::GetTypeColor() const
{ 
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ParameterCollectionInstance").ToFColor(true); 
}

void FAssetTypeActions_NiagaraParameterCollectionInstance::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Instance = Cast<UNiagaraParameterCollectionInstance>(*ObjIt);
		if (Instance != NULL)
		{
			TSharedRef< FNiagaraParameterCollectionToolkit > NewNiagaraNPCToolkit(new FNiagaraParameterCollectionToolkit());
			NewNiagaraNPCToolkit->Initialize(Mode, EditWithinLevelEditor, Instance);
		}
	}
}

UClass* FAssetTypeActions_NiagaraParameterCollectionInstance::GetSupportedClass() const
{
	return UNiagaraParameterCollectionInstance::StaticClass();
}

#undef LOCTEXT_NAMESPACE