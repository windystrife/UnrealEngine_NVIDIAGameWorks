// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_GameplayAbilitiesBlueprint.h"
#include "Misc/MessageDialog.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "GameplayAbilitiesEditor.h"
#include "GameplayAbilityBlueprint.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitiesBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_GameplayAbilitiesBlueprint::GetName() const
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GameplayAbilitiesBlueprint", "Gameplay Ability Blueprint"); 
}

FColor FAssetTypeActions_GameplayAbilitiesBlueprint::GetTypeColor() const
{
	return FColor(0, 96, 128);
}

void FAssetTypeActions_GameplayAbilitiesBlueprint::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Blueprint = Cast<UBlueprint>(*ObjIt);
		if (Blueprint && Blueprint->SkeletonGeneratedClass && Blueprint->GeneratedClass )
		{
			TSharedRef< FGameplayAbilitiesEditor > NewEditor(new FGameplayAbilitiesEditor());

			TArray<UBlueprint*> Blueprints;
			Blueprints.Add(Blueprint);

			NewEditor->InitGameplayAbilitiesEditor(Mode, EditWithinLevelEditor, Blueprints, ShouldUseDataOnlyEditor(Blueprint));
		}
		else
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("FailedToLoadAbilityBlueprint", "Gameplay Ability Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

bool FAssetTypeActions_GameplayAbilitiesBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_GameplayAbilitiesBlueprint::GetSupportedClass() const
{ 
	return UGameplayAbilityBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_GameplayAbilitiesBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UGameplayAbilitiesBlueprintFactory* GameplayAbilitiesBlueprintFactory = NewObject<UGameplayAbilitiesBlueprintFactory>();
	GameplayAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UGameplayAbility>(*InBlueprint->GeneratedClass);
	return GameplayAbilitiesBlueprintFactory;
}

#undef LOCTEXT_NAMESPACE
