// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GameplayTagAssetBase.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"

#include "GameplayTagContainer.h"
#include "SGameplayTagWidget.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_GameplayTagAssetBase::FAssetTypeActions_GameplayTagAssetBase(FName InTagPropertyName)
	: OwnedGameplayTagPropertyName(InTagPropertyName)
{}

bool FAssetTypeActions_GameplayTagAssetBase::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_GameplayTagAssetBase::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<UObject*> ContainerObjectOwners;
	TArray<FGameplayTagContainer*> Containers;
	for (int32 ObjIdx = 0; ObjIdx < InObjects.Num(); ++ObjIdx)
	{
		UObject* CurObj = InObjects[ObjIdx];
		if (CurObj)
		{
			UStructProperty* StructProp = FindField<UStructProperty>(CurObj->GetClass(), OwnedGameplayTagPropertyName);
			if(StructProp != NULL)
			{
				ContainerObjectOwners.Add(CurObj);
				Containers.Add(StructProp->ContainerPtrToValuePtr<FGameplayTagContainer>(CurObj));
			}
		}
	}

	ensure(Containers.Num() == ContainerObjectOwners.Num());
	if (Containers.Num() > 0 && (Containers.Num() == ContainerObjectOwners.Num()))
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("GameplayTags_Edit", "Edit Gameplay Tags..."),
			LOCTEXT("GameplayTags_EditToolTip", "Opens the Gameplay Tag Editor."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_GameplayTagAssetBase::OpenGameplayTagEditor, ContainerObjectOwners, Containers), FCanExecuteAction()));
	}
}

void FAssetTypeActions_GameplayTagAssetBase::OpenGameplayTagEditor(TArray<UObject*> Objects, TArray<FGameplayTagContainer*> Containers)
{
	TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
	for (int32 ObjIdx = 0; ObjIdx < Objects.Num() && ObjIdx < Containers.Num(); ++ObjIdx)
	{
		EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(Objects[ObjIdx], Containers[ObjIdx]));
	}

	FText Title;
	FText AssetName;

	const int32 NumAssets = EditableContainers.Num();
	if (NumAssets > 1)
	{
		AssetName = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseMultipleAssets", "{0} Assets"), FText::AsNumber( NumAssets ) );
		Title = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseEditorTitle", "Tag Editor: Owned Gameplay Tags: {0}"), AssetName );
	}
	else if (NumAssets > 0 && EditableContainers[0].TagContainerOwner.IsValid())
	{
		AssetName = FText::FromString( EditableContainers[0].TagContainerOwner->GetName() );
		Title = FText::Format( LOCTEXT("AssetTypeActions_GameplayTagAssetBaseEditorTitle", "Tag Editor: Owned Gameplay Tags: {0}"), AssetName );
	}

	TSharedPtr<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(600, 400))
		[
			SNew(SGameplayTagWidget, EditableContainers)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window.ToSharedRef());
	}
}

uint32 FAssetTypeActions_GameplayTagAssetBase::GetCategories()
{ 
	return EAssetTypeCategories::Misc; 
}

#undef LOCTEXT_NAMESPACE
