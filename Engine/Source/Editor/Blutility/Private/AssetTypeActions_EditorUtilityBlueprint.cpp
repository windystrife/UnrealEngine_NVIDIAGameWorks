// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_EditorUtilityBlueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "EditorUtilityBlueprintFactory.h"
#include "GlobalEditorUtilityBase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintEditorModule.h"
#include "GlobalBlutilityDialog.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/////////////////////////////////////////////////////
// FAssetTypeActions_EditorUtilityBlueprint

FText FAssetTypeActions_EditorUtilityBlueprint::GetName() const
{
	return LOCTEXT("AssetTypeActions_EditorUtilityBlueprint", "Blutility");
}

FColor FAssetTypeActions_EditorUtilityBlueprint::GetTypeColor() const
{
	return FColor(0, 169, 255);
}

UClass* FAssetTypeActions_EditorUtilityBlueprint::GetSupportedClass() const
{
	return UEditorUtilityBlueprint::StaticClass();
}

bool FAssetTypeActions_EditorUtilityBlueprint::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_EditorUtilityBlueprint::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Blueprints = GetTypedWeakObjectPtrs<UEditorUtilityBlueprint>(InObjects);

	/*
	if (Blueprints.Num() == 1)
	{
		if (UEditorUtilityBlueprint* Blueprint = Blueprints[0].Get())
		{
			if (Blueprint->ParentClass->IsChildOf(UGlobalEditorUtilityBase::StaticClass()))
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("BlutilityExecute", "Run Blutility"),
					LOCTEXT("BlutilityExecute_Tooltip", "Run this blutility."),
					NAME_None,
					FUIAction(
					FExecuteAction::CreateSP( this, &FAssetTypeActions_EditorUtilityBlueprint::ExecuteGlobalBlutility, Blueprints[0] ),
						FCanExecuteAction()
						)
					);
			}
		}
	}
	*/

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Blutility_Edit", "Edit Blueprint"),
		LOCTEXT("Blutility_EditTooltip", "Opens the selected blueprints in the full blueprint editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_EditorUtilityBlueprint::ExecuteEdit, Blueprints ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Blutility_EditDefaults", "Edit Defaults"),
		LOCTEXT("Blutility_EditDefaultsTooltip", "Edits the default properties for the selected blueprints."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_EditorUtilityBlueprint::ExecuteEditDefaults, Blueprints ),
			FCanExecuteAction()
			)
		);

	if (Blueprints.Num() == 1)
	{
		TAttribute<FText>::FGetter DynamicTooltipGetter;
		DynamicTooltipGetter.BindSP(this, &FAssetTypeActions_EditorUtilityBlueprint::GetNewDerivedBlueprintTooltip, Blueprints[0]);
		TAttribute<FText> DynamicTooltipAttribute = TAttribute<FText>::Create(DynamicTooltipGetter);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Blueprint_NewDerivedBlueprint", "Create Child Blueprint Class"),
			DynamicTooltipAttribute,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetTypeActions_EditorUtilityBlueprint::ExecuteNewDerivedBlueprint, Blueprints[0] ),
				FCanExecuteAction::CreateSP( this, &FAssetTypeActions_EditorUtilityBlueprint::CanExecuteNewDerivedBlueprint, Blueprints[0] )
				)
			);
	}
}

void FAssetTypeActions_EditorUtilityBlueprint::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UEditorUtilityBlueprint* Blueprint = Cast<UEditorUtilityBlueprint>(*ObjIt))
		{
			if (Blueprint->GeneratedClass->IsChildOf(UGlobalEditorUtilityBase::StaticClass()))
			{
				const UGlobalEditorUtilityBase* CDO = Blueprint->GeneratedClass->GetDefaultObject<UGlobalEditorUtilityBase>();
				if (CDO->bAutoRunDefaultAction)
				{
					// This is an instant-run blueprint, just execute it
					UGlobalEditorUtilityBase* Instance = NewObject<UGlobalEditorUtilityBase>(GetTransientPackage(), Blueprint->GeneratedClass);
					Instance->ExecuteDefaultAction();
				}
				else
				{
					// This one needs settings or has multiple actions to execute, so invoke the blutility dialog
					TSharedRef<FGlobalBlutilityDialog> NewBlutilityDialog(new FGlobalBlutilityDialog());
					NewBlutilityDialog->InitBlutilityDialog(Mode, EditWithinLevelEditor, Blueprint);
				}
			}
			else
			{
				// Edit actor blutilities
				FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>( "Kismet" );
				TSharedRef<IBlueprintEditor> NewBlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(Mode, EditWithinLevelEditor, Blueprint, false);
			}
		}
	}
}

uint32 FAssetTypeActions_EditorUtilityBlueprint::GetCategories()
{
	return EAssetTypeCategories::Blueprint;
}

void FAssetTypeActions_EditorUtilityBlueprint::ExecuteEdit(FWeakBlueprintPointerArray Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = (*ObjIt).Get())
		{
			FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
			TSharedRef<IBlueprintEditor> NewBlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Object, false);
		}
	}
}

void FAssetTypeActions_EditorUtilityBlueprint::ExecuteEditDefaults(FWeakBlueprintPointerArray Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (auto Object = (*ObjIt).Get())
		{
			FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
			TSharedRef<IBlueprintEditor> NewBlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Object, true);
		}
	}
}

void FAssetTypeActions_EditorUtilityBlueprint::ExecuteNewDerivedBlueprint(TWeakObjectPtr<UEditorUtilityBlueprint> InObject)
{
	if (auto Object = InObject.Get())
	{
		// The menu option should ONLY be available if there is only one blueprint selected, validated by the menu creation code
		UBlueprint* TargetBP = Object;
		UClass* TargetClass = TargetBP->GeneratedClass;

		if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(TargetClass))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("InvalidClassToMakeBlueprintFrom", "Invalid class with which to make a Blueprint."));
			return;
		}

		FString Name;
		FString PackageName;
		CreateUniqueAssetName(Object->GetOutermost()->GetName(), TEXT("_Child"), PackageName, Name);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		UEditorUtilityBlueprintFactory* BlueprintFactory = NewObject<UEditorUtilityBlueprintFactory>();
		BlueprintFactory->ParentClass = TargetClass;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().CreateNewAsset(Name, PackagePath, UEditorUtilityBlueprint::StaticClass(), BlueprintFactory);
	}
}

FText FAssetTypeActions_EditorUtilityBlueprint::GetNewDerivedBlueprintTooltip(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject)
{
	if(InObject->GeneratedClass->HasAnyClassFlags(CLASS_Deprecated))
	{
		return LOCTEXT("Blutility_NewDerivedBlueprintIsDeprecatedTooltip", "Blueprint class is deprecated, cannot derive a child Blueprint!");
	}
	else
	{
		return LOCTEXT("Blutility_NewDerivedBlueprintTooltip", "Creates a Child Blueprint Class based on the current Blueprint, allowing you to create variants easily.");
	}
}

bool FAssetTypeActions_EditorUtilityBlueprint::CanExecuteNewDerivedBlueprint(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject)
{
	return !InObject->GeneratedClass->HasAnyClassFlags(CLASS_Deprecated);
}

void FAssetTypeActions_EditorUtilityBlueprint::ExecuteGlobalBlutility(TWeakObjectPtr<UEditorUtilityBlueprint> InObject)
{
	if (auto Object = InObject.Get())
	{
		// The menu option should ONLY be available if there is only one blueprint selected, validated by the menu creation code
		UBlueprint* TargetBP = Object;
		UClass* TargetClass = TargetBP->GeneratedClass;

		if (!TargetClass->IsChildOf(UGlobalEditorUtilityBase::StaticClass()))
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GlobalBlUtilitiesOnly", "Can only invoke global blutilities."));
			return;
		}

		// Launch the blutility
		TArray<UObject*> ObjList;
		ObjList.Add(Object);
		OpenAssetEditor(ObjList, NULL);
	}
}

#undef LOCTEXT_NAMESPACE
