// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameProjectGenerationModule.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorStyleSet.h"
#include "GameProjectGenerationLog.h"
#include "GameProjectUtils.h"
#include "SGameProjectDialog.h"
#include "SNewClassDialog.h"
#include "TemplateCategory.h"
#include "SourceCodeNavigation.h"


IMPLEMENT_MODULE( FGameProjectGenerationModule, GameProjectGeneration );
DEFINE_LOG_CATEGORY(LogGameProjectGeneration);

#define LOCTEXT_NAMESPACE "GameProjectGeneration"

FName FTemplateCategory::BlueprintCategoryName = "Blueprint";
FName FTemplateCategory::CodeCategoryName = "C++";

void FGameProjectGenerationModule::StartupModule()
{
	RegisterTemplateCategory(
		FTemplateCategory::BlueprintCategoryName,
		LOCTEXT("BlueprintCategory_Name", "Blueprint"),
		LOCTEXT("BlueprintCategory_Description", "Blueprint templates require no programming knowledge.\nAll game mechanics can be implemented using Blueprint visual scripting.\nEach template includes a basic set of blueprints to use as a starting point for your game."),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintIcon"),
		FEditorStyle::GetBrush("GameProjectDialog.BlueprintImage"));

	RegisterTemplateCategory(
		FTemplateCategory::CodeCategoryName,
		LOCTEXT("CodeCategory_Name", "C++"),
		FText::Format(
			LOCTEXT("CodeCategory_Description", "C++ templates offer a good example of how to work with some of the core concepts of the Engine from code.\nYou still have the option of adding your own blueprints to the project at a later date if you want.\nChoosing this template type requires you to have {0} installed."),
			FSourceCodeNavigation::GetSuggestedSourceCodeIDE()
		),
		FEditorStyle::GetBrush("GameProjectDialog.CodeIcon"),
		FEditorStyle::GetBrush("GameProjectDialog.CodeImage"));
}


void FGameProjectGenerationModule::ShutdownModule()
{
	
}


TSharedRef<class SWidget> FGameProjectGenerationModule::CreateGameProjectDialog(bool bAllowProjectOpening, bool bAllowProjectCreate)
{
	return SNew(SGameProjectDialog)
		.AllowProjectOpening(bAllowProjectOpening)
		.AllowProjectCreate(bAllowProjectCreate);
}


TSharedRef<class SWidget> FGameProjectGenerationModule::CreateNewClassDialog(const UClass* InClass)
{
	return SNew(SNewClassDialog).Class(InClass);
}

void FGameProjectGenerationModule::OpenAddCodeToProjectDialog(const FAddToProjectConfig& Config)
{
	GameProjectUtils::OpenAddToProjectDialog(Config, EClassDomain::Native);
	AddCodeToProjectDialogOpenedEvent.Broadcast();
}

void FGameProjectGenerationModule::OpenAddBlueprintToProjectDialog(const FAddToProjectConfig& Config)
{
	GameProjectUtils::OpenAddToProjectDialog(Config, EClassDomain::Blueprint);
}

void FGameProjectGenerationModule::TryMakeProjectFileWriteable(const FString& ProjectFile)
{
	GameProjectUtils::TryMakeProjectFileWriteable(ProjectFile);
}

void FGameProjectGenerationModule::CheckForOutOfDateGameProjectFile()
{
	GameProjectUtils::CheckForOutOfDateGameProjectFile();
}


bool FGameProjectGenerationModule::UpdateGameProject(const FString& ProjectFile, const FString& EngineIdentifier, FText& OutFailReason)
{
	return GameProjectUtils::UpdateGameProject(ProjectFile, EngineIdentifier, OutFailReason);
}


bool FGameProjectGenerationModule::UpdateCodeProject(FText& OutFailReason, FText& OutFailLog)
{
	FScopedSlowTask SlowTask(0, LOCTEXT( "UpdatingCodeProject", "Updating code project..." ) );
	SlowTask.MakeDialog();

	return GameProjectUtils::GenerateCodeProjectFiles(FPaths::GetProjectFilePath(), OutFailReason, OutFailLog);
}

bool FGameProjectGenerationModule::GenerateBasicSourceCode(TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	return GameProjectUtils::GenerateBasicSourceCode(OutCreatedFiles, OutFailReason);
}

bool FGameProjectGenerationModule::ProjectHasCodeFiles()
{
	return GameProjectUtils::ProjectHasCodeFiles();
}

bool FGameProjectGenerationModule::ProjectRequiresBuild(const FName InPlatformName)
{
	return GameProjectUtils::ProjectRequiresBuild(InPlatformName);
}

FString FGameProjectGenerationModule::DetermineModuleIncludePath(const FModuleContextInfo& ModuleInfo, const FString& FileRelativeTo)
{
	return GameProjectUtils::DetermineModuleIncludePath(ModuleInfo, FileRelativeTo);
}

TArray<FModuleContextInfo> FGameProjectGenerationModule::GetCurrentProjectModules()
{
	return GameProjectUtils::GetCurrentProjectModules();
}

bool FGameProjectGenerationModule::IsValidBaseClassForCreation(const UClass* InClass, const FModuleContextInfo& InModuleInfo)
{
	return GameProjectUtils::IsValidBaseClassForCreation(InClass, InModuleInfo);
}

bool FGameProjectGenerationModule::IsValidBaseClassForCreation(const UClass* InClass, const TArray<FModuleContextInfo>& InModuleInfoArray)
{
	return GameProjectUtils::IsValidBaseClassForCreation(InClass, InModuleInfoArray);
}

void FGameProjectGenerationModule::GetProjectSourceDirectoryInfo(int32& OutNumFiles, int64& OutDirectorySize)
{
	GameProjectUtils::GetProjectSourceDirectoryInfo(OutNumFiles, OutDirectorySize);
}

void FGameProjectGenerationModule::CheckAndWarnProjectFilenameValid()
{
	GameProjectUtils::CheckAndWarnProjectFilenameValid();
}


void FGameProjectGenerationModule::UpdateSupportedTargetPlatforms(const FName& InPlatformName, const bool bIsSupported)
{
	GameProjectUtils::UpdateSupportedTargetPlatforms(InPlatformName, bIsSupported);
}

void FGameProjectGenerationModule::ClearSupportedTargetPlatforms()
{
	GameProjectUtils::ClearSupportedTargetPlatforms();
}

bool FGameProjectGenerationModule::RegisterTemplateCategory(FName Type, FText Name, FText Description, const FSlateBrush* Icon, const FSlateBrush* Image)
{
	if (TemplateCategories.Contains(Type))
	{
		return false;
	}

	FTemplateCategory Category = { Name, Description, Icon, Image, Type };
	TemplateCategories.Add(Type, MakeShareable(new FTemplateCategory(Category)));
	return true;
}

void FGameProjectGenerationModule::UnRegisterTemplateCategory(FName Type)
{
	TemplateCategories.Remove(Type);
}

#undef LOCTEXT_NAMESPACE
