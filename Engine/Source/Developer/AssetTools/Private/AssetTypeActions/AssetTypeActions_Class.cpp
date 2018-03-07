// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Class.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetData.h"
#include "HAL/FileManager.h"
#include "EditorStyleSet.h"
#include "SourceCodeNavigation.h"
#include "AddToProjectConfig.h"
#include "GameProjectGenerationModule.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

bool FAssetTypeActions_Class::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

void FAssetTypeActions_Class::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	UClass *const BaseClass = (InObjects.Num() == 1) ? Cast<UClass>(InObjects[0]) : nullptr;

	// Only allow the New class option if we have a base class that we can actually derive from in one of our project modules
	FGameProjectGenerationModule& GameProjectGenerationModule = FGameProjectGenerationModule::Get();
	TArray<FModuleContextInfo> ProjectModules = GameProjectGenerationModule.GetCurrentProjectModules();
	const bool bIsValidBaseCppClass = BaseClass && GameProjectGenerationModule.IsValidBaseClassForCreation(BaseClass, ProjectModules);
	const bool bIsValidBaseBlueprintClass = BaseClass && FKismetEditorUtilities::CanCreateBlueprintOfClass(BaseClass);

	auto CreateCreateDerivedCppClass = [BaseClass]()
	{
		// Work out where the header file for the current class is, as we'll use that path as the default for the new class
		FString BaseClassPath;
		if(FSourceCodeNavigation::FindClassHeaderPath(BaseClass, BaseClassPath))
		{
			// Strip off the actual filename as we only need the path
			BaseClassPath = FPaths::GetPath(BaseClassPath);
		}

		FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog(
			FAddToProjectConfig()
			.ParentClass(BaseClass)
			.InitialPath(BaseClassPath)
			.ParentWindow(FGlobalTabmanager::Get()->GetRootWindow())
		);
	};

	auto CanCreateDerivedCppClass = [bIsValidBaseCppClass]() -> bool
	{
		return bIsValidBaseCppClass;
	};

	auto CreateCreateDerivedBlueprintClass = [BaseClass]()
	{
		FGameProjectGenerationModule::Get().OpenAddBlueprintToProjectDialog(
			FAddToProjectConfig()
			.ParentClass(BaseClass)
			.ParentWindow(FGlobalTabmanager::Get()->GetRootWindow())
		);
	};

	auto CanCreateDerivedBlueprintClass = [bIsValidBaseBlueprintClass]() -> bool
	{
		return bIsValidBaseBlueprintClass;
	};

	FText NewDerivedCppClassLabel;
	FText NewDerivedCppClassToolTip;
	FText NewDerivedBlueprintClassLabel;
	FText NewDerivedBlueprintClassToolTip;
	if(InObjects.Num() == 1)
	{
		check(BaseClass);

		const FText BaseClassName = FText::FromName(BaseClass->GetFName());

		NewDerivedCppClassLabel = FText::Format(LOCTEXT("Class_NewDerivedCppClassLabel_CreateFrom", "Create C++ class derived from {0}"), BaseClassName);
		if(bIsValidBaseCppClass)
		{
			NewDerivedCppClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedCppClassTooltip_CreateFrom", "Create a new C++ class deriving from {0}."), BaseClassName);
		}
		else
		{
			NewDerivedCppClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedCppClassTooltip_InvalidClass", "Cannot create a new C++ class deriving from {0}."), BaseClassName);
		}

		NewDerivedBlueprintClassLabel = FText::Format(LOCTEXT("Class_NewDerivedBlueprintClassLabel_CreateFrom", "Create Blueprint class based on {0}"), BaseClassName);
		if(bIsValidBaseBlueprintClass)
		{
			NewDerivedBlueprintClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedBlueprintClassTooltip_CreateFrom", "Create a new Blueprint class based on {0}."), BaseClassName);
		}
		else
		{
			NewDerivedBlueprintClassToolTip = FText::Format(LOCTEXT("Class_NewDerivedBlueprintClassTooltip_InvalidClass", "Cannot create a new Blueprint class based on {0}."), BaseClassName);
		}
	}
	else
	{
		NewDerivedCppClassLabel = LOCTEXT("Class_NewDerivedCppClassLabel_InvalidNumberOfBases", "New C++ class derived from...");
		NewDerivedCppClassToolTip = LOCTEXT("Class_NewDerivedCppClassTooltip_InvalidNumberOfBases", "Can only create a derived C++ class when there is a single base class selected.");

		NewDerivedBlueprintClassLabel = LOCTEXT("Class_NewDerivedBlueprintClassLabel_InvalidNumberOfBases", "New Blueprint class based on...");
		NewDerivedBlueprintClassToolTip = LOCTEXT("Class_NewDerivedBlueprintClassTooltip_InvalidNumberOfBases", "Can only create a Blueprint class when there is a single base class selected.");
	}

	MenuBuilder.AddMenuEntry(
		NewDerivedCppClassLabel,
		NewDerivedCppClassToolTip,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "MainFrame.AddCodeToProject"),
		FUIAction(
			FExecuteAction::CreateLambda(CreateCreateDerivedCppClass),
			FCanExecuteAction::CreateLambda(CanCreateDerivedCppClass)
			)
		);

	MenuBuilder.AddMenuEntry(
		NewDerivedBlueprintClassLabel,
		NewDerivedBlueprintClassToolTip,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.CreateClassBlueprint"),
		FUIAction(
			FExecuteAction::CreateLambda(CreateCreateDerivedBlueprintClass),
			FCanExecuteAction::CreateLambda(CanCreateDerivedBlueprintClass)
			)
		);
}

UThumbnailInfo* FAssetTypeActions_Class::GetThumbnailInfo(UObject* Asset) const
{
	// todo: jdale - CLASS - We need to generate and store proper thumbnail info for classes so that we can store their custom render transforms
	// This can't be stored in the UClass instance (like we do for Blueprints), so we'll need another place to store it
	// This will need to be accessible to FClassThumbnailScene::GetSceneThumbnailInfo
	return nullptr;
}

void FAssetTypeActions_Class::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	for(UObject* Object : InObjects)
	{
		UClass* const Class = Cast<UClass>(Object);
		if(Class)
		{
			TArray<FString> FilesToOpen;

			FString ClassHeaderPath;
			if(FSourceCodeNavigation::FindClassHeaderPath(Class, ClassHeaderPath))
			{
				const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassHeaderPath);
				FilesToOpen.Add(AbsoluteHeaderPath);
			}

			FString ClassSourcePath;
			if(FSourceCodeNavigation::FindClassSourcePath(Class, ClassSourcePath))
			{
				const FString AbsoluteSourcePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassSourcePath);
				FilesToOpen.Add(AbsoluteSourcePath);
			}

			FSourceCodeNavigation::OpenSourceFiles(FilesToOpen);
		}
	}
}

TWeakPtr<IClassTypeActions> FAssetTypeActions_Class::GetClassTypeActions(const FAssetData& AssetData) const
{
	UClass* Class = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetName.ToString());
	if(Class)
	{
		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		return AssetToolsModule.Get().GetClassTypeActionsForClass(Class);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
