// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Builders/CubeBuilder.h"
#include "GameFramework/PlayerStart.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "FileHelpers.h"
#include "ProjectDescriptor.h"
#include "Private/IContentSource.h"
#include "GameProjectUtils.h"
#include "Editor/GameProjectGeneration/Private/SNewProjectWizard.h"

#include "DesktopPlatformModule.h"
#include "Tests/AutomationTestSettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor/GameProjectGeneration/Private/TemplateCategory.h"
#include "Editor/GameProjectGeneration/Private/TemplateItem.h"

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogGameProjectGenerationTests, Log, All);

namespace GameProjectAutomationUtils
{
	/**
	* Generates the desired project file name
	*/
	static FString GetDesiredProjectFilename()
	{
		UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
		check(AutomationTestSettings);

		FString ProjectName;
		const FString ProjectNameOverride = AutomationTestSettings->BuildPromotionTest.NewProjectSettings.NewProjectNameOverride;
		if (ProjectNameOverride.Len())
		{
			ProjectName = ProjectNameOverride;
		}
		else
		{
			ProjectName = TEXT("NewTestProject");
		}

		FString ProjectPath;
		const FString ProjectPathOverride = AutomationTestSettings->BuildPromotionTest.NewProjectSettings.NewProjectFolderOverride.Path;
		if (ProjectPathOverride.Len())
		{
			ProjectPath = ProjectPathOverride;
		}
		else
		{
			ProjectPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FAutomationTestFramework::Get().GetUserAutomationDirectory());
		}


		const FString Filename = ProjectName + TEXT(".") + FProjectDescriptor::GetExtension();
		FString ProjectFilename = FPaths::Combine(*ProjectPath, *ProjectName, *Filename);
		FPaths::MakePlatformFilename(ProjectFilename);

		return ProjectFilename;
	}


	/* 
	 * Create a project from a templalte with a given criteria
	 * @oaram	InTemplates			List of available project templates
	 * @param	InTargetedHardware	Target hardware (EHardwareClass)
	 * @param	InGraphicPreset		Graphics preset (EGraphicsPreset)
	 * @param	InCategory			Target category (EContentSourceCategory)
	 * @param	bInCopyStarterContent Should the starter content be copied also
	 * @param	OutMatchedProjects	Total projects matching criteria
	 * @param	OutCreatedProjects	Total projects succesfully created
	 */
	static void CreateProjectSet(TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& InTemplates, EHardwareClass::Type InTargetedHardware, EGraphicsPreset::Type InGraphicPreset, EContentSourceCategory InCategory, bool bInCopyStarterContent, int32 &OutCreatedProjects, int32 &OutMatchedProjects)
	{		
		// If this is empty, it will use the same name for each project, otherwise it will create a project based on target platform and source template
		FString TestRootFolder;// = "ProjectTests";
		
		// This has the code remove the projects once created
		bool bRemoveCreatedProjects = true;		
		OutCreatedProjects = 0;
		OutMatchedProjects = 0;
		UEnum* SourceCategoryEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EContentSourceCategory"));

		// The category name in the FTemplateItem is not the same as the enum definition EContentSourceCategory - convert it
		FName CategoryName;
		if( InCategory == EContentSourceCategory::BlueprintFeature)
		{
			CategoryName = FTemplateCategory::BlueprintCategoryName;
		}
		else if (InCategory == EContentSourceCategory::CodeFeature)
		{
			CategoryName = FTemplateCategory::CodeCategoryName;
		}
		else
		{
			// We didn't match a category
			if (SourceCategoryEnum != nullptr)
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed! Unknown category type %s"), *SourceCategoryEnum->GetNameStringByValue(int64(InCategory)));
			}
			else
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed! Unknown category type %d"), (int32)(InCategory));
			}
			
			return;
		}

		// Iterate all templates and try to create those that match the required criteria
		for (auto EachTemplate : InTemplates)
		{
			FName Name = EachTemplate.Key;						
			// Check this is the correct category
			if (Name == CategoryName)
			{
				// Now iterate each template in the category
				for (TSharedPtr<FTemplateItem> OneTemplate : EachTemplate.Value)
				{				
					TSharedPtr<FTemplateItem> Item = OneTemplate;

					// If this template is flagged as not for creation don't try to create it
					if( Item->ProjectFile.IsEmpty())
						continue;

					FString DesiredProjectFilename;
					if( TestRootFolder.IsEmpty() == true)
					{
						// Same name for all
						DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();
					}
					else
					{
						// Unique names
						FString Hardware;
						if (InTargetedHardware == EHardwareClass::Desktop)
						{
							Hardware = TEXT("Dsk");
						}
						else
						{
							Hardware = TEXT("Mob");
						}
						FString ProjectName = FPaths::GetCleanFilename(Item->ProjectFile).Replace(TEXT("TP_"), TEXT(""));
						FString ProjectDirName = FPaths::GetBaseFilename(Item->ProjectFile).Replace(TEXT("TP_"), TEXT(""));
						FString BasePath = FPaths::RootDir();
						DesiredProjectFilename = FString::Printf(TEXT("%s/%s/%s%s/%s%s"), *BasePath, *TestRootFolder, *Hardware, *ProjectDirName, *Hardware, *ProjectName);
					}

					// If the project already exists, delete it just in case things were left in a bad state.
					if ( IFileManager::Get().DirectoryExists(*FPaths::GetPath(DesiredProjectFilename)) )
					{
						IFileManager::Get().DeleteDirectory(*FPaths::GetPath(DesiredProjectFilename), /*RequireExists=*/false, /*Tree=*/true);
					}
					
					// Setup creation parameters
					FText FailReason, FailLog;
					FProjectInformation ProjectInfo(DesiredProjectFilename, Item->bGenerateCode, bInCopyStarterContent, Item->ProjectFile);
					ProjectInfo.TargetedHardware = InTargetedHardware;
					ProjectInfo.DefaultGraphicsPerformance = InGraphicPreset;
					TArray<FString> CreatedFiles;
					OutMatchedProjects++;

					// Finally try to create the project					
					if (!GameProjectUtils::CreateProject(ProjectInfo, FailReason, FailLog, &CreatedFiles))
					{
						// Failed, report the reason
						UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Failed to create %s project %s based on %s. Reason: %s\nProject Creation Failure Log:\n%s"), *SourceCategoryEnum->GetNameStringByValue((int64)InCategory), *DesiredProjectFilename, *Item->Name.ToString(), *FailReason.ToString(), *FailLog.ToString());
					}
					else
					{
						// Created ok
						OutCreatedProjects++;
						
						// Now remove the files we just created (if required)
						if(bRemoveCreatedProjects == true)						
						{
							FString RootFolder = FPaths::GetPath(DesiredProjectFilename);
							GameProjectUtils::DeleteCreatedFiles(RootFolder, CreatedFiles);
						}
					}
				}
			}
		}
		return;
	}
}

/**
* Automation test to clean up old test project files
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCleanupTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Cleanup Potential Project Location", EAutomationTestFlags::Disabled | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter );
bool FBuildPromotionNewProjectCleanupTest::RunTest(const FString& Parameters)
{
	FString DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();

	if (FPaths::FileExists(DesiredProjectFilename))
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Found an old project file at %s"), *DesiredProjectFilename);
		if (FPaths::IsProjectFilePathSet())
		{
			FString CurrentProjectPath = FPaths::GetProjectFilePath();
			if (CurrentProjectPath == DesiredProjectFilename)
			{
				UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("Can not clean up the target project location because it is the current active project."));
			}
			else
			{
				UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Removing files from old project path: %s"), *FPaths::GetPath(DesiredProjectFilename));
				bool bEnsureExists = false;
				bool bDeleteEntireTree = true;
				IFileManager::Get().DeleteDirectory(*FPaths::GetPath(DesiredProjectFilename), bEnsureExists, bDeleteEntireTree);
			}
		}
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Target project location is clear"));
	}

	return true;
}

/**
* Automation test to create a new project
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectCreateTest, "System.Promotion.Project Promotion Pass.Step 1 Blank Project Creation.Create Project", EAutomationTestFlags::Disabled | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::RequiresUser);
bool FBuildPromotionNewProjectCreateTest::RunTest(const FString& Parameters)
{
	FString DesiredProjectFilename = GameProjectAutomationUtils::GetDesiredProjectFilename();

	if (FPaths::FileExists(DesiredProjectFilename))
	{
		UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("A project already exists at the target location: %s"), *DesiredProjectFilename);
		const FString OldProjectFolder = FPaths::GetPath(DesiredProjectFilename);
		const FString OldProjectName = FPaths::GetBaseFilename(DesiredProjectFilename);
		const FString RootFolder = FPaths::GetPath(OldProjectFolder);

		//Add a number to the end
		for (uint32 i = 2;; ++i)
		{
			const FString PossibleProjectName = FString::Printf(TEXT("%s%i"), *OldProjectName, i);
			const FString PossibleProjectFilename = FString::Printf(TEXT("%s/%s/%s.%s"), *RootFolder, *PossibleProjectName, *PossibleProjectName, *FProjectDescriptor::GetExtension());
			if (!FPaths::FileExists(PossibleProjectFilename))
			{
				DesiredProjectFilename = PossibleProjectFilename;
				UE_LOG(LogGameProjectGenerationTests, Warning, TEXT("Changing the target project name to: %s"), *FPaths::GetBaseFilename(DesiredProjectFilename));
				break;
			}
		}
	}

	FText FailReason, FailLog;
	if (GameProjectUtils::CreateProject(FProjectInformation(DesiredProjectFilename, false, true), FailReason, FailLog))
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Generated a new project: %s"), *DesiredProjectFilename);
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test successful!"));
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("\nPlease switch to the new project and continue to Step 2."));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Could not generate new project: %s - %s"), *FailReason.ToString(), *FailLog.ToString());
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Test failed!"));
	}

	return true;
}

/**
* Automation test to create a simple level and save it
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildPromotionNewProjectMapTest, "System.Promotion.Project Promotion Pass.Step 2 Basic Level Creation.Create Basic Level", EAutomationTestFlags::Disabled | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
bool FBuildPromotionNewProjectMapTest::RunTest(const FString& Parameters)
{
	//New level
	UWorld* CurrentWorld = FAutomationEditorCommonUtils::CreateNewMap();
	if (!CurrentWorld)
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Failed to create an empty level"));
		return false;
	}

	UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Adding Level Geometry"));

	//Add some bsp and a player start
	GEditor->Exec(CurrentWorld, TEXT("BRUSH Scale 1 1 1"));
	for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); i++)
	{
		FLevelEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[i];
		if (!ViewportClient->IsOrtho())
		{
			ViewportClient->SetViewLocation(FVector(176, 2625, 2075));
			ViewportClient->SetViewRotation(FRotator(319, 269, 1));
		}
	}
	ULevel* CurrentLevel = CurrentWorld->GetCurrentLevel();

	//Cube Additive Brush
	UCubeBuilder* CubeAdditiveBrushBuilder = Cast<UCubeBuilder>(GEditor->FindBrushBuilder(UCubeBuilder::StaticClass()));
	CubeAdditiveBrushBuilder->X = 4096.0f;
	CubeAdditiveBrushBuilder->Y = 4096.0f;
	CubeAdditiveBrushBuilder->Z = 128.0f;
	CubeAdditiveBrushBuilder->Build(CurrentWorld);
	GEditor->Exec(CurrentWorld, TEXT("BRUSH MOVETO X=0 Y=0 Z=0"));
	GEditor->Exec(CurrentWorld, TEXT("BRUSH ADD"));

	//Add a playerstart
	const FTransform Transform(FRotator(-16384, 0, 0), FVector(0.f, 1750.f, 166.f));
	AActor* PlayerStart = GEditor->AddActor(CurrentWorld->GetCurrentLevel(), APlayerStart::StaticClass(), Transform);
	if (PlayerStart)
	{
		UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Added a player start"));
	}
	else
	{
		UE_LOG(LogGameProjectGenerationTests, Error, TEXT("Failed to add a player start"));
	}

	// Save the map
	FEditorFileUtils::SaveLevel(CurrentLevel, TEXT("/Game/Maps/NewProjectTest"));
	UE_LOG(LogGameProjectGenerationTests, Display, TEXT("Saved map"));

	return true;
}

/*
* Template project creation test
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreateBPTemplateProjectAutomationTests, "System.Promotion.Project Promotion Pass.Step 3 NewProjectCreationTests.CreateBlueprintProjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled)

/** 
 * Uses the new project wizard to locate all templates available for new blueprint project creation and verifies creation succeeds.
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FCreateBPTemplateProjectAutomationTests::RunTest(const FString& Parameters)
{
	TSharedPtr<SNewProjectWizard> NewProjectWizard;
	NewProjectWizard = SNew(SNewProjectWizard);
	
	TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& Templates = NewProjectWizard->FindTemplateProjects();
	int32 OutMatchedProjectsDesk = 0;
	int32 OutCreatedProjectsDesk = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Desktop, EGraphicsPreset::Maximum, EContentSourceCategory::BlueprintFeature, false, OutMatchedProjectsDesk, OutCreatedProjectsDesk);

	int32 OutMatchedProjectsMob = 0;
	int32 OutCreatedProjectsMob = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Mobile, EGraphicsPreset::Maximum, EContentSourceCategory::BlueprintFeature, false, OutMatchedProjectsMob, OutCreatedProjectsMob);
	
	return ( OutMatchedProjectsDesk == OutCreatedProjectsDesk ) && ( OutMatchedProjectsMob == OutCreatedProjectsMob  );
}

/*
* Template project creation test
*/
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreateCPPTemplateProjectAutomationTests, "System.Promotion.Project Promotion Pass.Step 3 NewProjectCreationTests.CreateCodeProjects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled)

/** 
 * Uses the new project wizard to locate all templates available for new code project creation and verifies creation succeeds.
 *
 * @param Parameters - Unused for this test
 * @return	TRUE if the test was successful, FALSE otherwise
 */
bool FCreateCPPTemplateProjectAutomationTests::RunTest(const FString& Parameters)
{
	TSharedPtr<SNewProjectWizard> NewProjectWizard;
	NewProjectWizard = SNew(SNewProjectWizard);
	//return ;
	TMap<FName, TArray<TSharedPtr<FTemplateItem>> >& Templates = NewProjectWizard->FindTemplateProjects();//GameProjectAutomationUtils::CreateTemplateList();
	int32 OutMatchedProjectsDesk = 0;
	int32 OutCreatedProjectsDesk = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Desktop, EGraphicsPreset::Maximum, EContentSourceCategory::CodeFeature, false, OutMatchedProjectsDesk, OutCreatedProjectsDesk);

	int32 OutMatchedProjectsMob = 0;
	int32 OutCreatedProjectsMob = 0;
	GameProjectAutomationUtils::CreateProjectSet(Templates, EHardwareClass::Mobile, EGraphicsPreset::Maximum, EContentSourceCategory::CodeFeature, false, OutMatchedProjectsMob, OutCreatedProjectsMob);

	return (OutMatchedProjectsDesk == OutCreatedProjectsDesk) && (OutMatchedProjectsMob == OutCreatedProjectsMob);

}

#endif //WITH_DEV_AUTOMATION_TESTS
