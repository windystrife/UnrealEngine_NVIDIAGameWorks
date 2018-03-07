// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "GameProjectUtils.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "FeaturePackContentSource.h"
#include "TemplateProjectDefs.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/Character.h"
#include "Misc/FeedbackContext.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/GameModeBase.h"
#include "UnrealEdMisc.h"
#include "PluginDescriptor.h"
#include "Interfaces/IPluginManager.h"
#include "ProjectDescriptor.h"
#include "Interfaces/IProjectManager.h"
#include "GameProjectGenerationLog.h"
#include "DefaultTemplateProjectDefs.h"
#include "SNewClassDialog.h"
#include "FeaturedClasses.inl"

#include "Interfaces/IMainFrameModule.h"

#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

#include "DesktopPlatformModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

#include "Styling/SlateIconFinder.h"
#include "SourceCodeNavigation.h"

#include "Misc/UProjectInfo.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/HotReloadInterface.h"

#include "Dialogs/SOutputLogDialog.h"

#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundEffectSource.h"
#include "Components/SynthComponent.h"

#include "PlatformInfo.h"
#include "Blueprint/BlueprintSupport.h"
#include "Settings/ProjectPackagingSettings.h"

#define LOCTEXT_NAMESPACE "GameProjectUtils"

#define MAX_PROJECT_PATH_BUFFER_SPACE 130 // Leave a reasonable buffer of additional characters to account for files created in the content directory during or after project generation
#define MAX_PROJECT_NAME_LENGTH 20 // Enforce a reasonable project name length so the path is not too long for PLATFORM_MAX_FILEPATH_LENGTH
static_assert(PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE > 0, "File system path shorter than project creation buffer space.");

#define MAX_CLASS_NAME_LENGTH 32 // Enforce a reasonable class name length so the path is not too long for PLATFORM_MAX_FILEPATH_LENGTH

TWeakPtr<SNotificationItem> GameProjectUtils::UpdateGameProjectNotification = NULL;
TWeakPtr<SNotificationItem> GameProjectUtils::WarningProjectNameNotification = NULL;

FString GameProjectUtils::DefaultFeaturePackExtension(TEXT(".upack"));	

FText FNewClassInfo::GetClassName() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? BaseClass->GetDisplayNameText() : FText::GetEmpty();

	case EClassType::EmptyCpp:
		return LOCTEXT("NoParentClass", "None");

	case EClassType::SlateWidget:
		return LOCTEXT("SlateWidgetParentClass", "Slate Widget");

	case EClassType::SlateWidgetStyle:
		return LOCTEXT("SlateWidgetStyleParentClass", "Slate Widget Style");

	case EClassType::UInterface:
		return LOCTEXT("UInterfaceParentClass", "Unreal Interface");

	default:
		break;
	}

	return FText::GetEmpty();
}

FText FNewClassInfo::GetClassDescription(const bool bFullDescription/* = true*/) const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		{
			if(BaseClass)
			{
				FString ClassDescription = BaseClass->GetToolTipText(/*bShortTooltip=*/!bFullDescription).ToString();

				if(!bFullDescription)
				{
					int32 FullStopIndex = 0;
					if(ClassDescription.FindChar('.', FullStopIndex))
					{
						// Only show the first sentence so as not to clutter up the UI with a detailed description of implementation details
						ClassDescription = ClassDescription.Left(FullStopIndex + 1);
					}

					// Strip out any new-lines in the description
					ClassDescription.ReplaceInline(TEXT("\n"), TEXT(" "));	
				}

				return FText::FromString(ClassDescription);
			}
		}
		break;

	case EClassType::EmptyCpp:
		return LOCTEXT("EmptyClassDescription", "An empty C++ class with a default constructor and destructor.");

	case EClassType::SlateWidget:
		return LOCTEXT("SlateWidgetClassDescription", "A custom Slate widget, deriving from SCompoundWidget.");

	case EClassType::SlateWidgetStyle:
		return LOCTEXT("SlateWidgetStyleClassDescription", "A custom Slate widget style, deriving from FSlateWidgetStyle, along with its associated UObject wrapper class.");

	case EClassType::UInterface:
		return LOCTEXT("UInterfaceClassDescription", "A UObject Interface class, to be implemented by other UObject-based classes.");

	default:
		break;
	}

	return FText::GetEmpty();
}

const FSlateBrush* FNewClassInfo::GetClassIcon() const
{
	// Safe to do even if BaseClass is null, since FindIconForClass will return the default icon
	return FSlateIconFinder::FindIconBrushForClass(BaseClass);
}

FString FNewClassInfo::GetClassPrefixCPP() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? BaseClass->GetPrefixCPP() : TEXT("");

	case EClassType::EmptyCpp:
		return TEXT("");

	case EClassType::SlateWidget:
		return TEXT("S");

	case EClassType::SlateWidgetStyle:
		return TEXT("F");

	case EClassType::UInterface:
		return TEXT("U");

	default:
		break;
	}
	return TEXT("");
}

FString FNewClassInfo::GetClassNameCPP() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? BaseClass->GetName() : TEXT("");

	case EClassType::EmptyCpp:
		return TEXT("");

	case EClassType::SlateWidget:
		return TEXT("CompoundWidget");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle");

	case EClassType::UInterface:
		return TEXT("Interface");

	default:
		break;
	}
	return TEXT("");
}

FString FNewClassInfo::GetCleanClassName(const FString& ClassName) const
{
	FString CleanClassName = ClassName;

	switch(ClassType)
	{
	case EClassType::SlateWidgetStyle:
		{
			// Slate widget style classes always take the form FMyThingWidget, and UMyThingWidgetStyle
			// if our class ends with either Widget or WidgetStyle, we need to strip those out to avoid silly looking duplicates
			if(CleanClassName.EndsWith(TEXT("Style")))
			{
				CleanClassName = CleanClassName.LeftChop(5); // 5 for "Style"
			}
			if(CleanClassName.EndsWith(TEXT("Widget")))
			{
				CleanClassName = CleanClassName.LeftChop(6); // 6 for "Widget"
			}
		}
		break;

	default:
		break;
	}

	return CleanClassName;
}

FString FNewClassInfo::GetFinalClassName(const FString& ClassName) const
{
	const FString CleanClassName = GetCleanClassName(ClassName);

	switch(ClassType)
	{
	case EClassType::SlateWidgetStyle:
		return FString::Printf(TEXT("%sWidgetStyle"), *CleanClassName);

	default:
		break;
	}

	return CleanClassName;
}

bool FNewClassInfo::GetIncludePath(FString& OutIncludePath) const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		if(BaseClass && BaseClass->HasMetaData(TEXT("IncludePath")))
		{
			OutIncludePath = BaseClass->GetMetaData(TEXT("IncludePath"));
			return true;
		}
		break;

	case EClassType::SlateWidget:
		OutIncludePath = "Widgets/SCompoundWidget.h";
		return true;

	case EClassType::SlateWidgetStyle:
		OutIncludePath = "Styling/SlateWidgetStyle.h";
		return true;

	default:
		break;
	}
	return false;
}

FString FNewClassInfo::GetBaseClassHeaderFilename() const
{
	FString IncludePath;

	switch (ClassType)
	{
	case EClassType::UObject:
		if (BaseClass)
		{
			FString ClassHeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(BaseClass, ClassHeaderPath) && IFileManager::Get().FileSize(*ClassHeaderPath) != INDEX_NONE)
			{
				return ClassHeaderPath;
			}
		}
		break;

	case EClassType::SlateWidget:
	case EClassType::SlateWidgetStyle:
		GetIncludePath(IncludePath);
		return FPaths::EngineDir() / TEXT("Source") / TEXT("Runtime") / TEXT("SlateCore") / TEXT("Public") / IncludePath;
	default:
		return FString();
	}

	return FString();
}

FString FNewClassInfo::GetHeaderFilename(const FString& ClassName) const
{
	const FString HeaderFilename = GetFinalClassName(ClassName) + TEXT(".h");

	switch(ClassType)
	{
	case EClassType::SlateWidget:
		return TEXT("S") + HeaderFilename;

	default:
		break;
	}

	return HeaderFilename;
}

FString FNewClassInfo::GetSourceFilename(const FString& ClassName) const
{
	const FString SourceFilename = GetFinalClassName(ClassName) + TEXT(".cpp");

	switch(ClassType)
	{
	case EClassType::SlateWidget:
		return TEXT("S") + SourceFilename;

	default:
		break;
	}

	return SourceFilename;
}

FString FNewClassInfo::GetHeaderTemplateFilename() const
{
	switch(ClassType)
	{
		case EClassType::UObject:
		{
			if (BaseClass != nullptr)
			{
				if ((BaseClass == UActorComponent::StaticClass()) || (BaseClass == USceneComponent::StaticClass()))
				{
					return TEXT("ActorComponentClass.h.template");
				}
				else if (BaseClass == AActor::StaticClass())
				{
					return TEXT("ActorClass.h.template");
				}
				else if (BaseClass == APawn::StaticClass())
				{
					return TEXT("PawnClass.h.template");
				}
				else if (BaseClass == ACharacter::StaticClass())
				{
					return TEXT("CharacterClass.h.template");
				}
				else if (BaseClass == USoundEffectSourcePreset::StaticClass())
				{
					return TEXT("SoundEffectSourceClass.h.template");
				}
				else if (BaseClass == USoundEffectSubmixPreset::StaticClass())
				{
					return TEXT("SoundEffectSubmixClass.h.template");
				}
				else if (BaseClass == USynthComponent::StaticClass())
				{
					return TEXT("SynthComponentClass.h.template");
				}
			}
			// Some other non-actor, non-component UObject class
			return TEXT( "UObjectClass.h.template" );
		}

	case EClassType::EmptyCpp:
		return TEXT("EmptyClass.h.template");

	case EClassType::SlateWidget:
		return TEXT("SlateWidget.h.template");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle.h.template");

	case EClassType::UInterface:
		return TEXT("InterfaceClass.h.template");

	default:
		break;
	}
	return TEXT("");
}

FString FNewClassInfo::GetSourceTemplateFilename() const
{
	switch(ClassType)
	{
		case EClassType::UObject:
			if (BaseClass != nullptr)
			{
				if ((BaseClass == UActorComponent::StaticClass()) || (BaseClass == USceneComponent::StaticClass()))
				{
					return TEXT("ActorComponentClass.cpp.template");
				}
				else if (BaseClass == AActor::StaticClass())
				{
					return TEXT("ActorClass.cpp.template");
				}
				else if (BaseClass == APawn::StaticClass())
				{
					return TEXT("PawnClass.cpp.template");
				}
				else if (BaseClass == ACharacter::StaticClass())
				{
					return TEXT("CharacterClass.cpp.template");
				}
				else if (BaseClass == USoundEffectSubmixPreset::StaticClass())
				{
					return TEXT("SoundEffectSubmixClass.cpp.template");
				}
				else if (BaseClass == USoundEffectSourcePreset::StaticClass())
				{
					return TEXT("SoundEffectSourceClass.cpp.template");
				}
				else if (BaseClass == USynthComponent::StaticClass())
				{
					return TEXT("SynthComponentClass.cpp.template");
				}
			}
			// Some other non-actor, non-component UObject class
			return TEXT( "UObjectClass.cpp.template" );
	
	case EClassType::EmptyCpp:
		return TEXT("EmptyClass.cpp.template");

	case EClassType::SlateWidget:
		return TEXT("SlateWidget.cpp.template");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle.cpp.template");

	case EClassType::UInterface:
		return TEXT("InterfaceClass.cpp.template");

	default:
		break;
	}
	return TEXT("");
}

bool GameProjectUtils::IsValidProjectFileForCreation(const FString& ProjectFile, FText& OutFailReason)
{
	const FString BaseProjectFile = FPaths::GetBaseFilename(ProjectFile);
	if ( FPaths::GetPath(ProjectFile).IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectPath", "You must specify a path." );
		return false;
	}

	if ( BaseProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectName", "You must specify a project name." );
		return false;
	}

	if ( BaseProjectFile.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsSpace", "Project names may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(BaseProjectFile[0]) )
	{
		OutFailReason = LOCTEXT( "ProjectNameMustBeginWithACharacter", "Project names must begin with an alphabetic character." );
		return false;
	}

	if ( BaseProjectFile.Len() > MAX_PROJECT_NAME_LENGTH )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectNameLength"), MAX_PROJECT_NAME_LENGTH );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameTooLong", "Project names must not be longer than {MaxProjectNameLength} characters." ), Args );
		return false;
	}

	const int32 MaxProjectPathLength = PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE;
	if ( FPaths::GetBaseFilename(ProjectFile, false).Len() > MaxProjectPathLength )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectPathLength"), MaxProjectPathLength );
		OutFailReason = FText::Format( LOCTEXT( "ProjectPathTooLong", "A project's path must not be longer than {MaxProjectPathLength} characters." ), Args );
		return false;
	}

	if ( FPaths::GetExtension(ProjectFile) != FProjectDescriptor::GetExtension() )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "InvalidProjectFileExtension", "File extension is not {ProjectFileExtension}" ), Args );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(BaseProjectFile, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameContainsIllegalCharacters", "Project names may not contain the following characters: {IllegalNameCharacters}" ), Args );
		return false;
	}

	if (NameContainsUnderscoreAndXB1Installed(BaseProjectFile))
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsIllegalCharactersOnXB1", "Project names may not contain an underscore when the Xbox One XDK is installed." );
		return false;
	}

	if ( !FPaths::ValidatePath(FPaths::GetPath(ProjectFile), &OutFailReason) )
	{
		return false;
	}

	if ( ProjectFileExists(ProjectFile) )
	{
		OutFailReason = LOCTEXT( "ProjectFileAlreadyExists", "This project file already exists." );
		return false;
	}

	if ( FPaths::ConvertRelativePathToFull(FPaths::GetPath(ProjectFile)).StartsWith( FPaths::ConvertRelativePathToFull(FPaths::EngineDir())) )
	{
		OutFailReason = LOCTEXT( "ProjectFileCannotBeUnderEngineFolder", "Project cannot be saved under the Engine folder. Please choose a different directory." );
		return false;
	}

	if ( AnyProjectFilesExistInFolder(FPaths::GetPath(ProjectFile)) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "AProjectFileAlreadyExistsAtLoction", "Another .{ProjectFileExtension} file already exists in the specified folder" ), Args );
		return false;
	}

	// Don't allow any files within target directory so we can safely delete everything on failure
	TArray<FString> ExistingFiles;
	IFileManager::Get().FindFiles(ExistingFiles, *(FPaths::GetPath(ProjectFile) / TEXT("*")), true, true);
	if (ExistingFiles.Num() > 0)
	{
		OutFailReason = LOCTEXT("ProjectFileCannotBeWithExistingFiles", "Project cannot be saved in a folder with existing files. Please choose a different directory/project name.");
		return false;
	}

	return true;
}

bool GameProjectUtils::OpenProject(const FString& ProjectFile, FText& OutFailReason)
{
	if ( ProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectFileSpecified", "You must specify a project file." );
		return false;
	}

	const FString BaseProjectFile = FPaths::GetBaseFilename(ProjectFile);
	if ( BaseProjectFile.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsSpace", "Project names may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(BaseProjectFile[0]) )
	{
		OutFailReason = LOCTEXT( "ProjectNameMustBeginWithACharacter", "Project names must begin with an alphabetic character." );
		return false;
	}

	const int32 MaxProjectPathLength = PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE;
	if ( FPaths::GetBaseFilename(ProjectFile, false).Len() > MaxProjectPathLength )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectPathLength"), MaxProjectPathLength );
		OutFailReason = FText::Format( LOCTEXT( "ProjectPathTooLong", "A project's path must not be longer than {MaxProjectPathLength} characters." ), Args );
		return false;
	}

	if ( FPaths::GetExtension(ProjectFile) != FProjectDescriptor::GetExtension() )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "InvalidProjectFileExtension", "File extension is not {ProjectFileExtension}" ), Args );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(BaseProjectFile, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameContainsIllegalCharacters", "Project names may not contain the following characters: {IllegalNameCharacters}" ), Args );
		return false;
	}

	if (NameContainsUnderscoreAndXB1Installed(BaseProjectFile))
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsIllegalCharactersOnXB1", "Project names may not contain an underscore when the Xbox One XDK is installed." );
		return false;
	}

	if ( !FPaths::ValidatePath(FPaths::GetPath(ProjectFile), &OutFailReason) )
	{
		return false;
	}

	if ( !ProjectFileExists(ProjectFile) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFile"), FText::FromString( ProjectFile ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectFileDoesNotExist", "{ProjectFile} does not exist." ), Args );
		return false;
	}

	FUnrealEdMisc::Get().SwitchProject(ProjectFile, false);

	return true;
}

bool GameProjectUtils::OpenCodeIDE(const FString& ProjectFile, FText& OutFailReason)
{
	if ( ProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectFileSpecified", "You must specify a project file." );
		return false;
	}

	// Check whether this project is a foreign project. Don't use the cached project dictionary; we may have just created a new project.
	FString SolutionFolder;
	FString SolutionFilenameWithoutExtension;
	if( FUProjectDictionary(FPaths::RootDir()).IsForeignProject(ProjectFile) )
	{
		SolutionFolder = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::GetPath(ProjectFile));
		SolutionFilenameWithoutExtension = FPaths::GetBaseFilename(ProjectFile);
	}
	else
	{
		SolutionFolder = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::RootDir());
		SolutionFilenameWithoutExtension = TEXT("UE4");
	}

	if (!FSourceCodeNavigation::OpenProjectSolution(FPaths::Combine(SolutionFolder, SolutionFilenameWithoutExtension)))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("AccessorName"), FSourceCodeNavigation::GetSelectedSourceCodeIDE());
		OutFailReason = FText::Format(LOCTEXT("OpenCodeIDE_FailedToOpen", "Failed to open selected source code accessor '{AccessorName}'"), Args);
		return false;
	}

	return true;
}

void GameProjectUtils::GetStarterContentFiles(TArray<FString>& OutFilenames)
{
	FString const SrcFolder = FPaths::FeaturePackDir();
	
	FString SearchPath = TEXT("*");
	SearchPath += DefaultFeaturePackExtension;
	IFileManager::Get().FindFilesRecursive(OutFilenames, *SrcFolder, *SearchPath, /*Files=*/true, /*Directories=*/false);
}

bool GameProjectUtils::CreateProject(const FProjectInformation& InProjectInfo, FText& OutFailReason, FText& OutFailLog, TArray<FString>* OutCreatedFiles)
{
	if ( !IsValidProjectFileForCreation(InProjectInfo.ProjectFilename, OutFailReason) )
	{
		return false;
	}

	FScopedSlowTask SlowTask(0, LOCTEXT( "CreatingProjectStatus", "Creating project..." ));
	SlowTask.MakeDialog();

	bool bProjectCreationSuccessful = false;
	FString TemplateName;
	if ( InProjectInfo.TemplateFile.IsEmpty() )
	{
		bProjectCreationSuccessful = GenerateProjectFromScratch(InProjectInfo, OutFailReason, OutFailLog);
		TemplateName = InProjectInfo.bShouldGenerateCode ? TEXT("Basic Code") : TEXT("Blank");
	}
	else
	{
		bProjectCreationSuccessful = CreateProjectFromTemplate(InProjectInfo, OutFailReason, OutFailLog, OutCreatedFiles);
		TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	}

	if (!bProjectCreationSuccessful && CleanupIsEnabled())
	{
		// Delete the new project folder
		const FString NewProjectFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);
		IFileManager::Get().DeleteDirectory(*NewProjectFolder, /*RequireExists=*/false, /*Tree=*/true);
		if( OutCreatedFiles != nullptr )
		{
			OutCreatedFiles->Empty();
		}
	}

	if( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Template"), TemplateName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectType"), InProjectInfo.bShouldGenerateCode ? TEXT("C++ Code") : TEXT("Content Only")));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), bProjectCreationSuccessful ? TEXT("Successful") : TEXT("Failed")));

		UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EHardwareClass"), true);
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HardwareClass"), Enum ? Enum->GetNameStringByValue(InProjectInfo.TargetedHardware) : FString()));
		Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGraphicsPreset"), true);
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("GraphicsPreset"), Enum ? Enum->GetNameStringByValue(InProjectInfo.DefaultGraphicsPerformance) : FString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("StarterContent"), InProjectInfo.bCopyStarterContent ? TEXT("Yes") : TEXT("No")));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.NewProject.ProjectCreated" ), EventAttributes );
	}

	return bProjectCreationSuccessful;
}

void GameProjectUtils::CheckForOutOfDateGameProjectFile()
{
	if ( FPaths::IsProjectFilePathSet() )
	{
		if (IProjectManager::Get().IsCurrentProjectDirty())
		{
			FText FailMessage;
			TryMakeProjectFileWriteable(FPaths::GetProjectFilePath());
			if (!IProjectManager::Get().SaveCurrentProjectToDisk(FailMessage))
			{
				FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
			}
		}

		FProjectStatus ProjectStatus;
		if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus))
		{
			if ( ProjectStatus.bRequiresUpdate )
			{
				const FText UpdateProjectText = LOCTEXT("UpdateProjectFilePrompt", "Project file is saved in an older format. Would you like to update it?");
				const FText UpdateProjectConfirmText = LOCTEXT("UpdateProjectFileConfirm", "Update");
				const FText UpdateProjectCancelText = LOCTEXT("UpdateProjectFileCancel", "Not Now");

				FNotificationInfo Info(UpdateProjectText);
				Info.bFireAndForget = false;
				Info.bUseLargeFont = false;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = false;
				Info.FadeOutDuration = 3.f;
				Info.ButtonDetails.Add(FNotificationButtonInfo(UpdateProjectConfirmText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnUpdateProjectConfirm)));
				Info.ButtonDetails.Add(FNotificationButtonInfo(UpdateProjectCancelText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnUpdateProjectCancel)));

				if (UpdateGameProjectNotification.IsValid())
				{
					UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
					UpdateGameProjectNotification.Reset();
				}

				UpdateGameProjectNotification = FSlateNotificationManager::Get().AddNotification(Info);

				if (UpdateGameProjectNotification.IsValid())
				{
					UpdateGameProjectNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}
		}

		// Check if there are any other updates we need to make to the project file
		if(!UpdateGameProjectNotification.IsValid())
		{
			const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
			if(Project != nullptr)
			{
				bool bUpdatePluginReferences = false;
				TArray<FPluginReferenceDescriptor> NewPluginReferences = Project->Plugins;

				// Check if there are any installed plugins which aren't referenced by the project file
				for(TSharedRef<IPlugin>& Plugin: IPluginManager::Get().GetEnabledPlugins())
				{
					if(Plugin->GetDescriptor().bInstalled && Project->FindPluginReferenceIndex(Plugin->GetName()) == INDEX_NONE)
					{
						FPluginReferenceDescriptor PluginReference(Plugin->GetName(), true);
						NewPluginReferences.Add(PluginReference);
						bUpdatePluginReferences = true;
					}
				}

				// Check if there are any referenced plugins that do not have a matching supported plugins list
				for(FPluginReferenceDescriptor& Reference: NewPluginReferences)
				{
					if(Reference.bEnabled)
					{
						TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Reference.Name);
						if(Plugin.IsValid())
						{
							const FPluginDescriptor& Descriptor = Plugin->GetDescriptor();
							if(Reference.MarketplaceURL != Descriptor.MarketplaceURL)
							{
								Reference.MarketplaceURL = Descriptor.MarketplaceURL;
								bUpdatePluginReferences = true;
							}
							if(Reference.SupportedTargetPlatforms != Descriptor.SupportedTargetPlatforms)
							{
								Reference.SupportedTargetPlatforms = Descriptor.SupportedTargetPlatforms;
								bUpdatePluginReferences = true;
							}
						}
					}
				}

				// Check if the file needs updating
				if(bUpdatePluginReferences)
				{
					UpdateProject(FProjectDescriptorModifier::CreateLambda( 
						[NewPluginReferences](FProjectDescriptor& Descriptor){ Descriptor.Plugins = NewPluginReferences; return true; }
					));
				}
			}
		}
	}
}

void GameProjectUtils::CheckAndWarnProjectFilenameValid()
{
	const FString& LoadedProjectFilePath = FPaths::IsProjectFilePathSet() ? FPaths::GetProjectFilePath() : FString();
	if ( !LoadedProjectFilePath.IsEmpty() )
	{
		const FString BaseProjectFile = FPaths::GetBaseFilename(LoadedProjectFilePath);
		if ( BaseProjectFile.Len() > MAX_PROJECT_NAME_LENGTH )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("MaxProjectNameLength"), MAX_PROJECT_NAME_LENGTH );
			const FText WarningReason = FText::Format( LOCTEXT( "WarnProjectNameTooLong", "Project names must not be longer than {MaxProjectNameLength} characters.\nYou might have problems saving or modifying a project with a longer name." ), Args );
			const FText WarningReasonOkText = LOCTEXT("WarningReasonOkText", "Ok");

			FNotificationInfo Info(WarningReason);
			Info.bFireAndForget = false;
			Info.bUseLargeFont = false;
			Info.bUseThrobber = false;
			Info.bUseSuccessFailIcons = false;
			Info.FadeOutDuration = 3.f;
			Info.ButtonDetails.Add(FNotificationButtonInfo(WarningReasonOkText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnWarningReasonOk)));

			if (WarningProjectNameNotification.IsValid())
			{
				WarningProjectNameNotification.Pin()->ExpireAndFadeout();
				WarningProjectNameNotification.Reset();
			}

			WarningProjectNameNotification = FSlateNotificationManager::Get().AddNotification(Info);

			if (WarningProjectNameNotification.IsValid())
			{
				WarningProjectNameNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
	}
}

void GameProjectUtils::OnWarningReasonOk()
{
	if ( WarningProjectNameNotification.IsValid() )
	{
		WarningProjectNameNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		WarningProjectNameNotification.Pin()->ExpireAndFadeout();
		WarningProjectNameNotification.Reset();
	}
}

bool GameProjectUtils::UpdateStartupModuleNames(FProjectDescriptor& Descriptor, const TArray<FString>* StartupModuleNames)
{
	if (StartupModuleNames == nullptr)
	{
		return false;
	}

	// Replace the modules names, if specified
	Descriptor.Modules.Empty();
	for (int32 Idx = 0; Idx < StartupModuleNames->Num(); Idx++)
	{
		Descriptor.Modules.Add(FModuleDescriptor(*(*StartupModuleNames)[Idx]));
	}

	return true;
}

bool GameProjectUtils::UpdateRequiredAdditionalDependencies(FProjectDescriptor& Descriptor, TArray<FString>& RequiredDependencies, const FString& ModuleName)
{
	bool bNeedsUpdate = false;

	for (auto& ModuleDesc : Descriptor.Modules)
	{
		if (ModuleDesc.Name != *ModuleName)
		{
			continue;
		}

		for (const auto& RequiredDep : RequiredDependencies)
		{
			if (!ModuleDesc.AdditionalDependencies.Contains(RequiredDep))
			{
				ModuleDesc.AdditionalDependencies.Add(RequiredDep);
				bNeedsUpdate = true;
			}
		}
	}

	return bNeedsUpdate;
}

bool GameProjectUtils::UpdateGameProject(const FString& ProjectFile, const FString& EngineIdentifier, FText& OutFailReason)
{
	return UpdateGameProjectFile(ProjectFile, EngineIdentifier, OutFailReason);
}

void GameProjectUtils::OpenAddToProjectDialog(const FAddToProjectConfig& Config, EClassDomain InDomain)
{
	// If we've been given a class then we only show the second page of the dialog, so we can make the window smaller as that page doesn't have as much content
	const FVector2D WindowSize = (Config._ParentClass) ? (InDomain == EClassDomain::Blueprint) ? FVector2D(940, 480) : FVector2D(940, 380) : FVector2D(940, 540);

	FText WindowTitle = Config._WindowTitle;
	if (WindowTitle.IsEmpty())
	{
		WindowTitle = InDomain == EClassDomain::Native ? LOCTEXT("AddCodeWindowHeader_Native", "Add C++ Class") : LOCTEXT("AddCodeWindowHeader_Blueprint", "Add Blueprint Class");
	}

	TSharedRef<SWindow> AddCodeWindow =
		SNew(SWindow)
		.Title( WindowTitle )
		.ClientSize( WindowSize )
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMinimize(false) .SupportsMaximize(false);

	TSharedRef<SNewClassDialog> NewClassDialog = 
		SNew(SNewClassDialog)
		.Class(Config._ParentClass)
		.ClassViewerFilter(Config._AllowableParents)
		.ClassDomain(InDomain)
		.FeaturedClasses(Config._FeaturedClasses)
		.InitialPath(Config._InitialPath)
		.OnAddedToProject( Config._OnAddedToProject )
		.DefaultClassPrefix( Config._DefaultClassPrefix )
		.DefaultClassName( Config._DefaultClassName );

	AddCodeWindow->SetContent( NewClassDialog );

	TSharedPtr<SWindow> ParentWindow = Config._ParentWindow;
	if (!ParentWindow.IsValid())
	{
		static const FName MainFrameModuleName = "MainFrame";
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrameModuleName);
		ParentWindow = MainFrameModule.GetParentWindow();
	}

	if (Config._bModal)
	{
		FSlateApplication::Get().AddModalWindow(AddCodeWindow, ParentWindow);
	}
	else if (ParentWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(AddCodeWindow, ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AddCodeWindow);
	}
}

bool GameProjectUtils::IsValidClassNameForCreation(const FString& NewClassName, FText& OutFailReason)
{
	if ( NewClassName.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoClassName", "You must specify a class name." );
		return false;
	}

	if ( NewClassName.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ClassNameContainsSpace", "Your class name may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(NewClassName[0]) )
	{
		OutFailReason = LOCTEXT( "ClassNameMustBeginWithACharacter", "Your class name must begin with an alphabetic character." );
		return false;
	}

	if ( NewClassName.Len() > MAX_CLASS_NAME_LENGTH )
	{
		OutFailReason = FText::Format( LOCTEXT( "ClassNameTooLong", "The class name must not be longer than {0} characters." ), FText::AsNumber(MAX_CLASS_NAME_LENGTH) );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(NewClassName, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ClassNameContainsIllegalCharacters", "The class name may not contain the following characters: '{IllegalNameCharacters}'" ), Args );
		return false;
	}

	return true;
}

bool GameProjectUtils::IsValidClassNameForCreation(const FString& NewClassName, const FModuleContextInfo& ModuleInfo, const TSet<FString>& DisallowedHeaderNames, FText& OutFailReason)
{
	if (!IsValidClassNameForCreation(NewClassName, OutFailReason))
	{
		return false;
	}
	
	// Look for a duplicate class in memory
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		if ( ClassIt->GetName() == NewClassName )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("NewClassName"), FText::FromString( NewClassName ) );
			OutFailReason = FText::Format( LOCTEXT("ClassNameAlreadyExists", "The name {NewClassName} is already used by another class."), Args );
			return false;
		}
	}

	// Look for a duplicate class on disk in their project
	{
		FString UnusedFoundPath;
		if ( FindSourceFileInProject(NewClassName + ".h", ModuleInfo.ModuleSourcePath, UnusedFoundPath) )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("NewClassName"), FText::FromString( NewClassName ) );
			OutFailReason = FText::Format( LOCTEXT("ClassNameAlreadyExists", "The name {NewClassName} is already used by another class."), Args );
			return false;
		}
	}

	// See if header name clashes with an engine header
	{
		FString UnusedFoundPath;
		if (DisallowedHeaderNames.Contains(NewClassName))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("NewHeaderName"), FText::FromString(NewClassName + ".h"));
			OutFailReason = FText::Format(LOCTEXT("HeaderNameAlreadyExists", "The file {NewHeaderName} already exists elsewhere in the engine."), Args);
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::IsValidBaseClassForCreation(const UClass* InClass, const FModuleContextInfo& InModuleInfo)
{
	auto DoesClassNeedAPIExport = [&InModuleInfo](const FString& InClassModuleName) -> bool
	{
		return InModuleInfo.ModuleName != InClassModuleName;
	};

	return IsValidBaseClassForCreation_Internal(InClass, FDoesClassNeedAPIExportCallback::CreateLambda(DoesClassNeedAPIExport));
}

bool GameProjectUtils::IsValidBaseClassForCreation(const UClass* InClass, const TArray<FModuleContextInfo>& InModuleInfoArray)
{
	auto DoesClassNeedAPIExport = [&InModuleInfoArray](const FString& InClassModuleName) -> bool
	{
		for(const FModuleContextInfo& ModuleInfo : InModuleInfoArray)
		{
			if(ModuleInfo.ModuleName == InClassModuleName)
			{
				return false;
			}
		}
		return true;
	};

	return IsValidBaseClassForCreation_Internal(InClass, FDoesClassNeedAPIExportCallback::CreateLambda(DoesClassNeedAPIExport));
}

bool GameProjectUtils::IsValidBaseClassForCreation_Internal(const UClass* InClass, const FDoesClassNeedAPIExportCallback& InDoesClassNeedAPIExport)
{
	// You may not make native classes based on blueprint generated classes
	const bool bIsBlueprintClass = (InClass->ClassGeneratedBy != nullptr);

	// UObject is special cased to be extensible since it would otherwise not be since it doesn't pass the API check (intrinsic class).
	const bool bIsExplicitlyUObject = (InClass == UObject::StaticClass());

	// You need API if you are not UObject itself, and you're in a module that was validated as needing API export
	const FString ClassModuleName = InClass->GetOutermost()->GetName().RightChop( FString(TEXT("/Script/")).Len() );
	const bool bNeedsAPI = !bIsExplicitlyUObject && InDoesClassNeedAPIExport.Execute(ClassModuleName);

	// You may not make a class that is not DLL exported.
	// MinimalAPI classes aren't compatible with the DLL export macro, but can still be used as a valid base
	const bool bHasAPI = InClass->HasAnyClassFlags(CLASS_RequiredAPI) || InClass->HasAnyClassFlags(CLASS_MinimalAPI);

	// @todo should we support interfaces?
	const bool bIsInterface = InClass->IsChildOf(UInterface::StaticClass());

	return !bIsBlueprintClass && (!bNeedsAPI || bHasAPI) && !bIsInterface;
}

GameProjectUtils::EAddCodeToProjectResult GameProjectUtils::AddCodeToProject(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason)
{
	const EAddCodeToProjectResult Result = AddCodeToProject_Internal(NewClassName, NewClassPath, ModuleInfo, ParentClassInfo, DisallowedHeaderNames, OutHeaderFilePath, OutCppFilePath, OutFailReason);

	if( FEngineAnalytics::IsAvailable() )
	{
		const FString ParentClassName = ParentClassInfo.GetClassNameCPP();

		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ParentClass"), ParentClassName.IsEmpty() ? TEXT("None") : ParentClassName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), Result == EAddCodeToProjectResult::Succeeded ? TEXT("Successful") : TEXT("Failed")));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FailureReason"), OutFailReason.ToString()));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.AddCodeToProject.CodeAdded" ), EventAttributes );
	}

	return Result;
}

UTemplateProjectDefs* GameProjectUtils::LoadTemplateDefs(const FString& ProjectDirectory)
{
	UTemplateProjectDefs* TemplateDefs = NULL;

	const FString TemplateDefsIniFilename = ProjectDirectory / TEXT("Config") / GetTemplateDefsFilename();
	if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*TemplateDefsIniFilename) )
	{
		UClass* ClassToConstruct = UDefaultTemplateProjectDefs::StaticClass();

		// see if template uses a custom project defs object
		FString ClassName;
		const bool bFoundValue = GConfig->GetString(*UTemplateProjectDefs::StaticClass()->GetPathName(), TEXT("TemplateProjectDefsClass"), ClassName, TemplateDefsIniFilename);
		if (bFoundValue && ClassName.Len() > 0)
		{
			UClass* OverrideClass = FindObject<UClass>(ANY_PACKAGE, *ClassName, false);
			if (nullptr != OverrideClass)
			{
				ClassToConstruct = OverrideClass;
			}
			else
			{
				UE_LOG(LogGameProjectGeneration, Error, TEXT("Failed to find template project defs class '%s', using default."), *ClassName);
			}
		}
		TemplateDefs = NewObject<UTemplateProjectDefs>(GetTransientPackage(), ClassToConstruct);
		TemplateDefs->LoadConfig(UTemplateProjectDefs::StaticClass(), *TemplateDefsIniFilename);
	}

	return TemplateDefs;
}

bool GameProjectUtils::GenerateProjectFromScratch(const FProjectInformation& InProjectInfo, FText& OutFailReason, FText& OutFailLog)
{
	FScopedSlowTask SlowTask(5);

	const FString NewProjectFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);
	const FString NewProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
	TArray<FString> CreatedFiles;

	SlowTask.EnterProgressFrame();

	// Generate config files
	if (!GenerateConfigFiles(InProjectInfo, CreatedFiles, OutFailReason))
	{
		return false;
	}

	// Insert any required feature packs (EG starter content) into ini file. These will be imported automatically when the editor is first run
	if(!InsertFeaturePacksIntoINIFile(InProjectInfo, OutFailReason))
	{
		return false;
	}
	
	// Make the Content folder
	const FString ContentFolder = NewProjectFolder / TEXT("Content");
	if ( !IFileManager::Get().MakeDirectory(*ContentFolder) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ContentFolder"), FText::FromString( ContentFolder ) );
		OutFailReason = FText::Format( LOCTEXT("FailedToCreateContentFolder", "Failed to create the content folder {ContentFolder}"), Args );
		return false;
	}

	SlowTask.EnterProgressFrame();

	TArray<FString> StartupModuleNames;
	if ( InProjectInfo.bShouldGenerateCode )
	{
		FScopedSlowTask LocalScope(2);

		LocalScope.EnterProgressFrame();
		// Generate basic source code files
		if ( !GenerateBasicSourceCode(NewProjectFolder / TEXT("Source"), NewProjectName, NewProjectFolder, StartupModuleNames, CreatedFiles, OutFailReason) )
		{
			return false;
		}

		LocalScope.EnterProgressFrame();
		// Generate game framework source code files
		if ( !GenerateGameFrameworkSourceCode(NewProjectFolder / TEXT("Source"), NewProjectName, CreatedFiles, OutFailReason) )
		{
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Generate the project file
	{
		// Set up the descriptor
		FProjectDescriptor Descriptor;
		for(int32 Idx = 0; Idx < StartupModuleNames.Num(); Idx++)
		{
			Descriptor.Modules.Add(FModuleDescriptor(*StartupModuleNames[Idx]));
		}

		Descriptor.bIsEnterpriseProject = InProjectInfo.bIsEnterpriseProject;

		// Try to save it
		FText LocalFailReason;
		if(!Descriptor.Save(InProjectInfo.ProjectFilename, LocalFailReason))
		{
			OutFailReason = LocalFailReason;
			return false;
		}
		CreatedFiles.Add(InProjectInfo.ProjectFilename);

		// Set the engine identifier for it. Do this after saving, so it can be correctly detected as foreign or non-foreign.
		if(!SetEngineAssociationForForeignProject(InProjectInfo.ProjectFilename, OutFailReason))
		{
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	if ( InProjectInfo.bShouldGenerateCode )
	{
		// Generate project files
		if ( !GenerateCodeProjectFiles(InProjectInfo.ProjectFilename, OutFailReason, OutFailLog) )
		{
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	UE_LOG(LogGameProjectGeneration, Log, TEXT("Created new project with %d files (plus project files)"), CreatedFiles.Num());
	return true;
}

bool GameProjectUtils::CreateProjectFromTemplate(const FProjectInformation& InProjectInfo, FText& OutFailReason, FText& OutFailLog,TArray<FString>* OutCreatedFiles)
{
	FScopedSlowTask SlowTask(10);

	const FString ProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
	const FString TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	const FString SrcFolder = FPaths::GetPath(InProjectInfo.TemplateFile);
	const FString DestFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);

	if ( !FPlatformFileManager::Get().GetPlatformFile().FileExists(*InProjectInfo.TemplateFile) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("TemplateFile"), FText::FromString( InProjectInfo.TemplateFile ) );
		OutFailReason = FText::Format( LOCTEXT("InvalidTemplate_MissingProject", "Template project \"{TemplateFile}\" does not exist."), Args );
		return false;
	}

	SlowTask.EnterProgressFrame();

	UTemplateProjectDefs* TemplateDefs = LoadTemplateDefs(SrcFolder);
	if ( TemplateDefs == NULL )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("TemplateFile"), FText::FromString( FPaths::GetBaseFilename(InProjectInfo.TemplateFile) ) );
		Args.Add( TEXT("TemplateDefinesFile"), FText::FromString( GetTemplateDefsFilename() ) );
		OutFailReason = FText::Format( LOCTEXT("InvalidTemplate_MissingDefs", "Template project \"{TemplateFile}\" does not have definitions file: '{TemplateDefinesFile}'."), Args );
		return false;
	}

	SlowTask.EnterProgressFrame();

	// Fix up the replacement strings using the specified project name
	TemplateDefs->FixupStrings(TemplateName, ProjectName);

	// Form a list of all extensions we care about
	TSet<FString> ReplacementsInFilesExtensions;
	for ( const FTemplateReplacement& Replacement : TemplateDefs->ReplacementsInFiles )
	{
		ReplacementsInFilesExtensions.Append(Replacement.Extensions);
	}

	// Keep a list of created files so we can delete them if project creation fails
	TArray<FString> CreatedFiles;

	SlowTask.EnterProgressFrame();

	// Discover and copy all files in the src folder to the destination, excluding a few files and folders
	TArray<FString> FilesToCopy;
	TArray<FString> FilesThatNeedContentsReplaced;
	TMap<FString, FString> ClassRenames;
	IFileManager::Get().FindFilesRecursive(FilesToCopy, *SrcFolder, TEXT("*"), /*Files=*/true, /*Directories=*/false);

	SlowTask.EnterProgressFrame();
	{
		// Open a new feedback scope for the loop so we can report how far through the copy we are
		FScopedSlowTask InnerSlowTask(FilesToCopy.Num());
		for ( const FString& SrcFilename : FilesToCopy )
		{
			// Update the progress
			FFormatNamedArguments Args;
			Args.Add( TEXT("SrcFilename"), FText::FromString( FPaths::GetCleanFilename(SrcFilename) ) );
			InnerSlowTask.EnterProgressFrame(1, FText::Format( LOCTEXT( "CreatingProjectStatus_CopyingFile", "Copying File {SrcFilename}..." ), Args ));

			// Get the file path, relative to the src folder
			const FString SrcFileSubpath = SrcFilename.RightChop(SrcFolder.Len() + 1);

			// Skip any files that were configured to be ignored
			if ( TemplateDefs->FilesToIgnore.Contains(SrcFileSubpath) )
			{
				// This file was marked as "ignored"
				continue;
			}

			// Skip any folders that were configured to be ignored
			if ( const FString* IgnoredFolder = TemplateDefs->FoldersToIgnore.FindByPredicate([&SrcFileSubpath](const FString& Ignore){ return SrcFileSubpath.StartsWith(Ignore + TEXT("/")); }) )
			{
				// This folder was marked as "ignored"
				UE_LOG(LogGameProjectGeneration, Verbose, TEXT("'%s': Skipping as it is in an ignored folder '%s'"), *SrcFilename, **IgnoredFolder);
				continue;
			}

			// Retarget any folders that were chosen to be renamed by choosing a new destination subpath now
			FString DestFileSubpathWithoutFilename = FPaths::GetPath(SrcFileSubpath) + TEXT("/");
			for ( const FTemplateFolderRename& FolderRename : TemplateDefs->FolderRenames )
			{
				if ( SrcFileSubpath.StartsWith(FolderRename.From + TEXT("/")) )
				{
					// This was a file in a renamed folder. Retarget to the new location
					DestFileSubpathWithoutFilename = FolderRename.To / DestFileSubpathWithoutFilename.RightChop( FolderRename.From.Len() );
					UE_LOG(LogGameProjectGeneration, Verbose, TEXT("'%s': Moving to '%s' as it matched folder rename ('%s'->'%s')"), *SrcFilename, *DestFileSubpathWithoutFilename, *FolderRename.From, *FolderRename.To);
				}
			}

			// Retarget any files that were chosen to have parts of their names replaced here
			FString DestBaseFilename = FPaths::GetBaseFilename(SrcFileSubpath);
			const FString FileExtension = FPaths::GetExtension(SrcFileSubpath);
			for ( const FTemplateReplacement& Replacement : TemplateDefs->FilenameReplacements )
			{
				if ( Replacement.Extensions.Contains( FileExtension ) )
				{
					// This file matched a filename replacement extension, apply it now
					FString LastDestBaseFilename = DestBaseFilename;
					DestBaseFilename = DestBaseFilename.Replace(*Replacement.From, *Replacement.To, Replacement.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);

					if (LastDestBaseFilename != DestBaseFilename)
					{
						UE_LOG(LogGameProjectGeneration, Verbose, TEXT("'%s': Renaming to '%s/%s' as it matched file rename ('%s'->'%s')"), *SrcFilename, *DestFileSubpathWithoutFilename, *DestBaseFilename, *Replacement.From, *Replacement.To);
					}
				}
			}

			// Perform the copy
			const FString DestFilename = DestFolder / DestFileSubpathWithoutFilename + DestBaseFilename + TEXT(".") + FileExtension;
			if ( IFileManager::Get().Copy(*DestFilename, *SrcFilename) == COPY_OK )
			{
				CreatedFiles.Add(DestFilename);

				if ( ReplacementsInFilesExtensions.Contains(FileExtension) )
				{
					FilesThatNeedContentsReplaced.Add(DestFilename);
				}

				// Allow project template to extract class renames from this file copy
				if (FPaths::GetBaseFilename(SrcFilename) != FPaths::GetBaseFilename(DestFilename)
					&& TemplateDefs->IsClassRename(DestFilename, SrcFilename, FileExtension))
				{
					// Looks like a UObject file!
					ClassRenames.Add(FPaths::GetBaseFilename(SrcFilename), FPaths::GetBaseFilename(DestFilename));
				}
			}
			else
			{
				FFormatNamedArguments FailArgs;
				FailArgs.Add(TEXT("SrcFilename"), FText::FromString(SrcFilename));
				FailArgs.Add(TEXT("DestFilename"), FText::FromString(DestFilename));
				OutFailReason = FText::Format(LOCTEXT("FailedToCopyFile", "Failed to copy \"{SrcFilename}\" to \"{DestFilename}\"."), FailArgs);
				return false;
			}
		}
	}

	SlowTask.EnterProgressFrame();
	{
		// Open a new feedback scope for the loop so we can report how far through the process we are
		FScopedSlowTask InnerSlowTask(FilesThatNeedContentsReplaced.Num());

		// Open all files with the specified extensions and replace text
		for ( const FString& FileToFix : FilesThatNeedContentsReplaced )
		{
			InnerSlowTask.EnterProgressFrame();

			bool bSuccessfullyProcessed = false;

			FString FileContents;
			if ( FFileHelper::LoadFileToString(FileContents, *FileToFix) )
			{
				for ( const FTemplateReplacement& Replacement : TemplateDefs->ReplacementsInFiles )
				{
					if ( Replacement.Extensions.Contains( FPaths::GetExtension(FileToFix) ) )
					{
						FileContents = FileContents.Replace(*Replacement.From, *Replacement.To, Replacement.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
					}
				}

				if ( FFileHelper::SaveStringToFile(FileContents, *FileToFix) )
				{
					bSuccessfullyProcessed = true;
				}
			}

			if ( !bSuccessfullyProcessed )
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("FileToFix"), FText::FromString( FileToFix ) );
				OutFailReason = FText::Format( LOCTEXT("FailedToFixUpFile", "Failed to process file \"{FileToFix}\"."), Args );
				return false;
			}
		}
	}

	SlowTask.EnterProgressFrame();

	const FString ProjectConfigPath = DestFolder / TEXT("Config");

	// Write out the hardware class target settings chosen for this project
	{
		const FString DefaultEngineIniFilename = ProjectConfigPath / TEXT("DefaultEngine.ini");

		FString FileContents;
		// Load the existing file - if it doesn't exist we create it
		FFileHelper::LoadFileToString(FileContents, *DefaultEngineIniFilename);

		FileContents += LINE_TERMINATOR;
		FileContents += GetHardwareConfigString(InProjectInfo);

		if ( !WriteOutputFile(DefaultEngineIniFilename, FileContents, OutFailReason) )
		{
			return false;
		}
	}

	// Fixup specific ini values
	TArray<FTemplateConfigValue> ConfigValuesToSet;
	TemplateDefs->AddConfigValues(ConfigValuesToSet, TemplateName, ProjectName, InProjectInfo.bShouldGenerateCode);
	ConfigValuesToSet.Emplace(TEXT("DefaultGame.ini"), TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), FGuid::NewGuid().ToString(), /*InShouldReplaceExistingValue=*/true);

	// Add all classname fixups
	for ( const TPair<FString, FString>& Rename : ClassRenames )
	{
		const FString ClassRedirectString = FString::Printf(TEXT("(OldClassName=\"%s\",NewClassName=\"%s\")"), *Rename.Key, *Rename.Value);
		ConfigValuesToSet.Emplace(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.Engine"), TEXT("+ActiveClassRedirects"), *ClassRedirectString, /*InShouldReplaceExistingValue=*/false);
	}

	// Fix all specified config values
	for ( const FTemplateConfigValue& ConfigValue : ConfigValuesToSet )
	{
		const FString IniFilename = ProjectConfigPath / ConfigValue.ConfigFile;
		bool bSuccessfullyProcessed = false;

		TArray<FString> FileLines;
		if ( FFileHelper::LoadANSITextFileToStrings(*IniFilename, &IFileManager::Get(), FileLines) )
		{
			FString FileOutput;
			const FString TargetSection = ConfigValue.ConfigSection;
			FString CurSection;
			bool bFoundTargetKey = false;
			for ( const FString& LineIn : FileLines )
			{
				FString Line = LineIn;
				Line.TrimStartAndEndInline();

				bool bShouldExcludeLineFromOutput = false;

				// If we not yet found the target key parse each line looking for it
				if ( !bFoundTargetKey )
				{
					// Check for an empty line. No work needs to be done on these lines
					if ( Line.Len() == 0 )
					{

					}
					// Comment lines start with ";". Skip these lines entirely.
					else if ( Line.StartsWith(TEXT(";")) )
					{
						
					}
					// If this is a section line, update the section
					else if ( Line.StartsWith(TEXT("[")) )
					{
						// If we are entering a new section and we have not yet found our key in the target section, add it to the end of the section
						if ( CurSection == TargetSection )
						{
							FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR + LINE_TERMINATOR;
							bFoundTargetKey = true;
						}

						// Update the current section
						CurSection = Line.Mid(1, Line.Len() - 2);
					}
					// This is possibly an actual key/value pair
					else if ( CurSection == TargetSection )
					{
						// Key value pairs contain an equals sign
						const int32 EqualsIdx = Line.Find(TEXT("="));
						if ( EqualsIdx != INDEX_NONE )
						{
							// Determine the key and see if it is the target key
							const FString Key = Line.Left(EqualsIdx);
							if ( Key == ConfigValue.ConfigKey )
							{
								// Found the target key, add it to the output and skip the current line if the target value is supposed to replace
								FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR;
								bShouldExcludeLineFromOutput = ConfigValue.bShouldReplaceExistingValue;
								bFoundTargetKey = true;
							}
						}
					}
				}

				// Unless we replaced the key, add this line to the output
				if ( !bShouldExcludeLineFromOutput )
				{
					FileOutput += Line;
					if ( &LineIn != &FileLines.Last() )
					{
						// Add a line terminator on every line except the last
						FileOutput += LINE_TERMINATOR;
					}
				}
			}

			// If the key did not exist, add it here
			if ( !bFoundTargetKey )
			{
				// If we did not end in the correct section, add the section to the bottom of the file
				if ( CurSection != TargetSection )
				{
					FileOutput += LINE_TERMINATOR;
					FileOutput += LINE_TERMINATOR;
					FileOutput += FString::Printf(TEXT("[%s]"), *TargetSection) + LINE_TERMINATOR;
				}

				// Add the key/value here
				FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR;
			}

			if ( FFileHelper::SaveStringToFile(FileOutput, *IniFilename) )
			{
				bSuccessfullyProcessed = true;
			}
		}

		if ( !bSuccessfullyProcessed )
		{
			OutFailReason = LOCTEXT("FailedToFixUpDefaultEngine", "Failed to process file DefaultEngine.ini");
			return false;
		}
	}

	// Insert any required feature packs (EG starter content) into ini file. These will be imported automatically when the editor is first run
	if(!InsertFeaturePacksIntoINIFile(InProjectInfo, OutFailReason))
	{
		return false;
	}

	if( !AddSharedContentToProject(InProjectInfo, CreatedFiles, OutFailReason ) )
	{
		return false;
	}
	
	
	SlowTask.EnterProgressFrame();

	// Generate the project file
	{
		// Load the source project
		FProjectDescriptor Project;
		if(!Project.Load(InProjectInfo.TemplateFile, OutFailReason))
		{
			return false;
		}

		// Update it to current
		Project.EngineAssociation.Empty();
		Project.EpicSampleNameHash = 0;

		Project.bIsEnterpriseProject = InProjectInfo.bIsEnterpriseProject; // Force the enterprise flag to the value that was requested in the ProjectInfo.

		// Fix up module names
		const FString BaseSourceName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
		const FString BaseNewName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
		for ( FModuleDescriptor& ModuleInfo : Project.Modules )
		{
			ModuleInfo.Name = FName(*ModuleInfo.Name.ToString().Replace(*BaseSourceName, *BaseNewName));
		}

		// Save it to disk
		if(!Project.Save(InProjectInfo.ProjectFilename, OutFailReason))
		{
			return false;
		}

		// Set the engine identifier if it's a foreign project. Do this after saving, so it can be correctly detected as foreign.
		if(!SetEngineAssociationForForeignProject(InProjectInfo.ProjectFilename, OutFailReason))
		{
			return false;
		}

		// Add it to the list of created files
		CreatedFiles.Add(InProjectInfo.ProjectFilename);
	}

	SlowTask.EnterProgressFrame();

	SlowTask.EnterProgressFrame();
	if ( InProjectInfo.bShouldGenerateCode )
	{
		// Generate project files
		if ( !GenerateCodeProjectFiles(InProjectInfo.ProjectFilename, OutFailReason, OutFailLog) )
		{
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	if (!TemplateDefs->PostGenerateProject(DestFolder, SrcFolder, InProjectInfo.ProjectFilename, InProjectInfo.TemplateFile, InProjectInfo.bShouldGenerateCode, OutFailReason))
	{
		return false;
	}
	
	if( OutCreatedFiles != nullptr )
	{
		OutCreatedFiles->Append(CreatedFiles);
	}
	return true;
}

bool GameProjectUtils::SetEngineAssociationForForeignProject(const FString& ProjectFileName, FText& OutFailReason)
{
	if(FUProjectDictionary(FPaths::RootDir()).IsForeignProject(ProjectFileName))
	{
		if(!FDesktopPlatformModule::Get()->SetEngineIdentifierForProject(ProjectFileName, FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier()))
		{
			OutFailReason = LOCTEXT("FailedToSetEngineIdentifier", "Couldn't set engine identifier for project");
			return false;
		}
	}
	return true;
}

FString GameProjectUtils::GetTemplateDefsFilename()
{
	return TEXT("TemplateDefs.ini");
}

bool GameProjectUtils::NameContainsOnlyLegalCharacters(const FString& TestName, FString& OutIllegalCharacters)
{
	bool bContainsIllegalCharacters = false;

	// Only allow alphanumeric characters in the project name
	bool bFoundAlphaNumericChar = false;
	for ( int32 CharIdx = 0 ; CharIdx < TestName.Len() ; ++CharIdx )
	{
		const FString& Char = TestName.Mid( CharIdx, 1 );
		if ( !FChar::IsAlnum(Char[0]) && Char != TEXT("_") )
		{
			if ( !OutIllegalCharacters.Contains( Char ) )
			{
				OutIllegalCharacters += Char;
			}

			bContainsIllegalCharacters = true;
		}
	}

	return !bContainsIllegalCharacters;
}

bool GameProjectUtils::NameContainsUnderscoreAndXB1Installed(const FString& TestName)
{
	// disabled for now so people with the SDK installed can use the editor
	return false;

	bool bContainsIllegalCharacters = false;

	// Only allow alphanumeric characters in the project name
	for ( int32 CharIdx = 0 ; CharIdx < TestName.Len() ; ++CharIdx )
	{
		const FString& Char = TestName.Mid( CharIdx, 1 );
		if ( Char == TEXT("_") )
		{
			const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(TEXT("XboxOne"));
			if (Platform)
			{
				FString NotInstalledDocLink;
				if (Platform->IsSdkInstalled(true, NotInstalledDocLink))
				{
					bContainsIllegalCharacters = true;
				}
			}
		}
	}

	return bContainsIllegalCharacters;
}

bool GameProjectUtils::ProjectFileExists(const FString& ProjectFile)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*ProjectFile);
}

bool GameProjectUtils::AnyProjectFilesExistInFolder(const FString& Path)
{
	TArray<FString> ExistingFiles;
	const FString Wildcard = FString::Printf(TEXT("%s/*.%s"), *Path, *FProjectDescriptor::GetExtension());
	IFileManager::Get().FindFiles(ExistingFiles, *Wildcard, /*Files=*/true, /*Directories=*/false);

	return ExistingFiles.Num() > 0;
}

bool GameProjectUtils::CleanupIsEnabled()
{
	// Clean up files when running Rocket (unless otherwise specified on the command line)
	return FParse::Param(FCommandLine::Get(), TEXT("norocketcleanup")) == false;
}

void GameProjectUtils::DeleteCreatedFiles(const FString& RootFolder, const TArray<FString>& CreatedFiles)
{
	if (CleanupIsEnabled())
	{
		for ( auto FileToDeleteIt = CreatedFiles.CreateConstIterator(); FileToDeleteIt; ++FileToDeleteIt )
		{
			IFileManager::Get().Delete(**FileToDeleteIt);
		}

		// If the project folder is empty after deleting all the files we created, delete the directory as well
		TArray<FString> RemainingFiles;
		IFileManager::Get().FindFilesRecursive(RemainingFiles, *RootFolder, TEXT("*.*"), /*Files=*/true, /*Directories=*/false);
		if ( RemainingFiles.Num() == 0 )
		{
			IFileManager::Get().DeleteDirectory(*RootFolder, /*RequireExists=*/false, /*Tree=*/true);
		}
	}
}

FString GameProjectUtils::GetHardwareConfigString(const FProjectInformation& InProjectInfo)
{
	FString HardwareTargeting;
	
	FString TargetHardwareAsString;
	UEnum::GetValueAsString(TEXT("/Script/HardwareTargeting.EHardwareClass"), InProjectInfo.TargetedHardware, /*out*/ TargetHardwareAsString);

	FString GraphicsPresetAsString;
	UEnum::GetValueAsString(TEXT("/Script/HardwareTargeting.EGraphicsPreset"), InProjectInfo.DefaultGraphicsPerformance, /*out*/ GraphicsPresetAsString);

	HardwareTargeting += TEXT("[/Script/HardwareTargeting.HardwareTargetingSettings]") LINE_TERMINATOR;
	HardwareTargeting += FString::Printf(TEXT("TargetedHardwareClass=%s") LINE_TERMINATOR, *TargetHardwareAsString);
	HardwareTargeting += FString::Printf(TEXT("DefaultGraphicsPerformance=%s") LINE_TERMINATOR, *GraphicsPresetAsString);
	HardwareTargeting += LINE_TERMINATOR;

	return HardwareTargeting;
}

bool GameProjectUtils::GenerateConfigFiles(const FProjectInformation& InProjectInfo, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString NewProjectFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);
	const FString NewProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);

	FString ProjectConfigPath = NewProjectFolder / TEXT("Config");

	// DefaultEngine.ini
	{
		const FString DefaultEngineIniFilename = ProjectConfigPath / TEXT("DefaultEngine.ini");
		FString FileContents;

		FileContents += TEXT("[URL]") LINE_TERMINATOR;

		FileContents += GetHardwareConfigString(InProjectInfo);
		FileContents += LINE_TERMINATOR;
		
		if (InProjectInfo.bCopyStarterContent)
		{
			FString SpecificEditorStartupMap;
			FString SpecificGameDefaultMap;

			// If we have starter content packs available, specify starter map
			if( IsStarterContentAvailableForNewProjects() == true )
			{
				if (InProjectInfo.TargetedHardware == EHardwareClass::Mobile)
				{
					SpecificEditorStartupMap = TEXT("/Game/MobileStarterContent/Maps/Minimal_Default");
					SpecificGameDefaultMap = TEXT("/Game/MobileStarterContent/Maps/Minimal_Default");
				}
				else
				{
					SpecificEditorStartupMap = TEXT("/Game/StarterContent/Maps/Minimal_Default");
					SpecificGameDefaultMap = TEXT("/Game/StarterContent/Maps/Minimal_Default");
				}
			}
			
			// Write out the settings for startup map and game default map
			FileContents += TEXT("[/Script/EngineSettings.GameMapsSettings]") LINE_TERMINATOR;
			FileContents += FString::Printf(TEXT("EditorStartupMap=%s") LINE_TERMINATOR, *SpecificEditorStartupMap);
			FileContents += FString::Printf(TEXT("GameDefaultMap=%s") LINE_TERMINATOR, *SpecificGameDefaultMap);
			if (InProjectInfo.bShouldGenerateCode)
			{
				FileContents += FString::Printf(TEXT("GlobalDefaultGameMode=\"/Script/%s.%sGameMode\"") LINE_TERMINATOR, *NewProjectName, *NewProjectName);
			}			
		}

		if (WriteOutputFile(DefaultEngineIniFilename, FileContents, OutFailReason))
		{
			OutCreatedFiles.Add(DefaultEngineIniFilename);
		}
		else
		{
			return false;
		}
	}

	// DefaultEditor.ini
	{
		const FString DefaultEditorIniFilename = ProjectConfigPath / TEXT("DefaultEditor.ini");
		FString FileContents;

		if (WriteOutputFile(DefaultEditorIniFilename, FileContents, OutFailReason))
		{
			OutCreatedFiles.Add(DefaultEditorIniFilename);
		}
		else
		{
			return false;
		}
	}

	// DefaultGame.ini
	{
		const FString DefaultGameIniFilename = ProjectConfigPath / TEXT("DefaultGame.ini");
		FString FileContents;
		FileContents += TEXT("[/Script/EngineSettings.GeneralProjectSettings]") LINE_TERMINATOR;
		FileContents += TEXT("ProjectID=") + FGuid::NewGuid().ToString() + LINE_TERMINATOR;

		if (WriteOutputFile(DefaultGameIniFilename, FileContents, OutFailReason))
		{
			OutCreatedFiles.Add(DefaultGameIniFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::GenerateBasicSourceCode(TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	TArray<FString> StartupModuleNames;
	if (GameProjectUtils::GenerateBasicSourceCode(FPaths::GameSourceDir().LeftChop(1), FApp::GetProjectName(), FPaths::ProjectDir(), StartupModuleNames, OutCreatedFiles, OutFailReason))
	{
		GameProjectUtils::UpdateProject(
			FProjectDescriptorModifier::CreateLambda(
			[&StartupModuleNames](FProjectDescriptor& Descriptor)
			{
				return UpdateStartupModuleNames(Descriptor, &StartupModuleNames);
			}));
		return true;
	}

	return false;
}

bool GameProjectUtils::GenerateBasicSourceCode(const FString& NewProjectSourcePath, const FString& NewProjectName, const FString& NewProjectRoot, TArray<FString>& OutGeneratedStartupModuleNames, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString GameModulePath = NewProjectSourcePath / NewProjectName;
	const FString EditorName = NewProjectName + TEXT("Editor");

	// MyGame.Build.cs
	{
		const FString NewBuildFilename = GameModulePath / NewProjectName + TEXT(".Build.cs");
		TArray<FString> PublicDependencyModuleNames;
		PublicDependencyModuleNames.Add(TEXT("Core"));
		PublicDependencyModuleNames.Add(TEXT("CoreUObject"));
		PublicDependencyModuleNames.Add(TEXT("Engine"));
		PublicDependencyModuleNames.Add(TEXT("InputCore"));
		TArray<FString> PrivateDependencyModuleNames;
		if ( GenerateGameModuleBuildFile(NewBuildFilename, NewProjectName, PublicDependencyModuleNames, PrivateDependencyModuleNames, OutFailReason) )
		{
			OutGeneratedStartupModuleNames.Add(NewProjectName);
			OutCreatedFiles.Add(NewBuildFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame.Target.cs
	{
		const FString NewTargetFilename = NewProjectSourcePath / NewProjectName + TEXT(".Target.cs");
		TArray<FString> ExtraModuleNames;
		ExtraModuleNames.Add( NewProjectName );
		if ( GenerateGameModuleTargetFile(NewTargetFilename, NewProjectName, ExtraModuleNames, OutFailReason) )
		{
			OutCreatedFiles.Add(NewTargetFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGameEditor.Target.cs
	{
		const FString NewTargetFilename = NewProjectSourcePath / EditorName + TEXT(".Target.cs");
		// Include the MyGame module...
		TArray<FString> ExtraModuleNames;
		ExtraModuleNames.Add(NewProjectName);
		if ( GenerateEditorModuleTargetFile(NewTargetFilename, EditorName, ExtraModuleNames, OutFailReason) )
		{
			OutCreatedFiles.Add(NewTargetFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame.h
	{
		const FString NewHeaderFilename = GameModulePath / NewProjectName + TEXT(".h");
		TArray<FString> PublicHeaderIncludes;
		if ( GenerateGameModuleHeaderFile(NewHeaderFilename, PublicHeaderIncludes, OutFailReason) )
		{
			OutCreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame.cpp
	{
		const FString NewCPPFilename = GameModulePath / NewProjectName + TEXT(".cpp");
		if ( GenerateGameModuleCPPFile(NewCPPFilename, NewProjectName, NewProjectName, OutFailReason) )
		{
			OutCreatedFiles.Add(NewCPPFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::GenerateGameFrameworkSourceCode(const FString& NewProjectSourcePath, const FString& NewProjectName, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString GameModulePath = NewProjectSourcePath / NewProjectName;

	// Used to override the code generation validation since the module we're creating isn't the same as the project we currently have loaded
	FModuleContextInfo NewModuleInfo;
	NewModuleInfo.ModuleName = NewProjectName;
	NewModuleInfo.ModuleType = EHostType::Runtime;
	NewModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(GameModulePath / ""); // Ensure trailing /

	// MyGameGameMode.h
	{
		const UClass* BaseClass = AGameModeBase::StaticClass();
		const FString NewClassName = NewProjectName + BaseClass->GetName();
		const FString NewHeaderFilename = GameModulePath / NewClassName + TEXT(".h");
		FString UnusedSyncLocation;
		if ( GenerateClassHeaderFile(NewHeaderFilename, NewClassName, FNewClassInfo(BaseClass), TArray<FString>(), TEXT(""), TEXT(""), UnusedSyncLocation, NewModuleInfo, false, OutFailReason) )
		{
			OutCreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGameGameMode.cpp
	{
		const UClass* BaseClass = AGameModeBase::StaticClass();
		const FString NewClassName = NewProjectName + BaseClass->GetName();
		const FString NewCPPFilename = GameModulePath / NewClassName + TEXT(".cpp");
		
		TArray<FString> PropertyOverrides;
		TArray<FString> AdditionalIncludes;
		FString UnusedSyncLocation;

		if ( GenerateClassCPPFile(NewCPPFilename, NewClassName, FNewClassInfo(BaseClass), AdditionalIncludes, PropertyOverrides, TEXT(""), UnusedSyncLocation, NewModuleInfo, OutFailReason) )
		{
			OutCreatedFiles.Add(NewCPPFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::BuildCodeProject(const FString& ProjectFilename)
{
	// Build the project while capturing the log output. Passing GWarn to CompileGameProject will allow Slate to display the progress bar.
	FStringOutputDevice OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	GLog->AddOutputDevice(&OutputLog);
	bool bCompileSucceeded = FDesktopPlatformModule::Get()->CompileGameProject(FPaths::RootDir(), ProjectFilename, GWarn);
	GLog->RemoveOutputDevice(&OutputLog);

	// Try to compile the modules
	if(!bCompileSucceeded)
	{
		FText DevEnvName = FSourceCodeNavigation::GetSelectedSourceCodeIDE();

		TArray<FText> CompileFailedButtons;
		int32 OpenIDEButton = CompileFailedButtons.Add(FText::Format(LOCTEXT("CompileFailedOpenIDE", "Open with {0}"), DevEnvName));
		CompileFailedButtons.Add(LOCTEXT("CompileFailedCancel", "Cancel"));

		FText LogText = FText::FromString(OutputLog.Replace(LINE_TERMINATOR, TEXT("\n")).TrimEnd());
		int32 CompileFailedChoice = SOutputLogDialog::Open(LOCTEXT("CompileFailedTitle", "Compile Failed"), FText::Format(LOCTEXT("CompileFailedHeader", "The project could not be compiled. Would you like to open it in {0}?"), DevEnvName), LogText, FText::GetEmpty(), CompileFailedButtons);

		FText FailReason;
		if(CompileFailedChoice == OpenIDEButton && !GameProjectUtils::OpenCodeIDE(ProjectFilename, FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
		}
	}
	return bCompileSucceeded;
}

bool GameProjectUtils::GenerateCodeProjectFiles(const FString& ProjectFilename, FText& OutFailReason, FText& OutFailLog)
{
	FStringOutputDevice OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	GLog->AddOutputDevice(&OutputLog);
	bool bHaveProjectFiles = FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), ProjectFilename, GWarn);
	GLog->RemoveOutputDevice(&OutputLog);

	if ( !bHaveProjectFiles )
	{
		OutFailReason = LOCTEXT("ErrorWhileGeneratingProjectFiles", "An error occurred while trying to generate project files.");
		OutFailLog = FText::FromString(OutputLog);
		return false;
	}

	return true;
}

bool GameProjectUtils::IsStarterContentAvailableForNewProjects()
{
	TArray<FString> StarterContentFiles;
	GetStarterContentFiles(StarterContentFiles);
	
	bool bHasStaterContent = StarterContentFiles.FindByPredicate([&](const FString& Str){ return Str.Contains("StarterContent"); }) != nullptr;
	return bHasStaterContent;
}

TArray<FModuleContextInfo> GameProjectUtils::GetCurrentProjectModules()
{
	const FProjectDescriptor* const CurrentProject = IProjectManager::Get().GetCurrentProject();
	check(CurrentProject);

	TArray<FModuleContextInfo> RetModuleInfos;

	if (!GameProjectUtils::ProjectHasCodeFiles() || CurrentProject->Modules.Num() == 0)
	{
		// If this project doesn't currently have any code in it, we need to add a dummy entry for the game
		// so that we can still use the class wizard (this module will be created once we add a class)
		FModuleContextInfo ModuleInfo;
		ModuleInfo.ModuleName = FApp::GetProjectName();
		ModuleInfo.ModuleType = EHostType::Runtime;
		ModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir() / ModuleInfo.ModuleName / ""); // Ensure trailing /
		RetModuleInfos.Emplace(ModuleInfo);
	}

	// Resolve out the paths for each module and add the cut-down into to our output array
	for (const FModuleDescriptor& ModuleDesc : CurrentProject->Modules)
	{
		FModuleContextInfo ModuleInfo;
		ModuleInfo.ModuleName = ModuleDesc.Name.ToString();
		ModuleInfo.ModuleType = ModuleDesc.Type;

		// Try and find the .Build.cs file for this module within our currently loaded project's Source directory
		FString TmpPath;
		if (!FindSourceFileInProject(ModuleInfo.ModuleName + ".Build.cs", FPaths::GameSourceDir(), TmpPath))
		{
			continue;
		}

		// Chop the .Build.cs file off the end of the path
		ModuleInfo.ModuleSourcePath = FPaths::GetPath(TmpPath);
		ModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(ModuleInfo.ModuleSourcePath / ""); // Ensure trailing /

		RetModuleInfos.Emplace(ModuleInfo);
	}

	return RetModuleInfos;
}

TArray<FModuleContextInfo> GameProjectUtils::GetCurrentProjectPluginModules()
{
	const FProjectDescriptor* const CurrentProject = IProjectManager::Get().GetCurrentProject();
	check(CurrentProject);

	TArray<FModuleContextInfo> RetModuleInfos;

	if (!GameProjectUtils::ProjectHasCodeFiles() || CurrentProject->Modules.Num() == 0)
	{
		// Don't get plugins if the game project has no source tree.
		return RetModuleInfos;
	}

	// Resolve out the paths for each module and add the cut-down into to our output array
	for (const auto& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		// Only get plugins that are a part of the game project
		if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project)
		{
			for (const auto& PluginModule : Plugin->GetDescriptor().Modules)
			{
				FModuleContextInfo ModuleInfo;
				ModuleInfo.ModuleName = PluginModule.Name.ToString();
				ModuleInfo.ModuleType = PluginModule.Type;

				// Try and find the .Build.cs file for this module within the plugin source tree
				FString TmpPath;
				if (!FindSourceFileInProject(ModuleInfo.ModuleName + ".Build.cs", Plugin->GetBaseDir(), TmpPath))
				{
					continue;
				}

				// Chop the .Build.cs file off the end of the path
				ModuleInfo.ModuleSourcePath = FPaths::GetPath(TmpPath);
				ModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(ModuleInfo.ModuleSourcePath / ""); // Ensure trailing /

				RetModuleInfos.Emplace(ModuleInfo);
			}
		}
	}

	return RetModuleInfos;
}

bool GameProjectUtils::IsValidSourcePath(const FString& InPath, const FModuleContextInfo& ModuleInfo, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /

	// Validate the path contains no invalid characters
	if(!FPaths::ValidatePath(AbsoluteInPath, OutFailReason))
	{
		return false;
	}

	if(!AbsoluteInPath.StartsWith(ModuleInfo.ModuleSourcePath))
	{
		if(OutFailReason)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ModuleName"), FText::FromString(ModuleInfo.ModuleName));
			Args.Add(TEXT("RootSourcePath"), FText::FromString(ModuleInfo.ModuleSourcePath));
			*OutFailReason = FText::Format( LOCTEXT("SourcePathInvalidForModule", "All source code for '{ModuleName}' must exist within '{RootSourcePath}'"), Args );
		}
		return false;
	}

	return true;
}

bool GameProjectUtils::CalculateSourcePaths(const FString& InPath, const FModuleContextInfo& ModuleInfo, FString& OutHeaderPath, FString& OutSourcePath, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /
	OutHeaderPath = AbsoluteInPath;
	OutSourcePath = AbsoluteInPath;

	EClassLocation ClassPathLocation = EClassLocation::UserDefined;
	if(!GetClassLocation(InPath, ModuleInfo, ClassPathLocation, OutFailReason))
	{
		return false;
	}

	const FString RootPath = ModuleInfo.ModuleSourcePath;
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /
	const FString ClassesPath = RootPath / "Classes" / "";		// Ensure trailing /

	// The root path must exist; we will allow the creation of sub-folders, but not the module root!
	// We ignore this check if the project doesn't already have source code in it, as the module folder won't yet have been created
	const bool bHasCodeFiles = GameProjectUtils::ProjectHasCodeFiles();
	if(!IFileManager::Get().DirectoryExists(*RootPath) && bHasCodeFiles)
	{
		if(OutFailReason)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ModuleSourcePath"), FText::FromString(RootPath));
			*OutFailReason = FText::Format(LOCTEXT("SourcePathMissingModuleRoot", "The specified module path does not exist on disk: {ModuleSourcePath}"), Args);
		}
		return false;
	}

	// The rules for placing header files are as follows:
	// 1) If InPath is the source root, and GetClassLocation has said the class header should be in the Public folder, put it in the Public folder
	// 2) Otherwise, just place the header at InPath (the default set above)
	if(AbsoluteInPath == RootPath)
	{
		OutHeaderPath = (ClassPathLocation == EClassLocation::Public) ? PublicPath : AbsoluteInPath;
	}

	// The rules for placing source files are as follows:
	// 1) If InPath is the source root, and GetClassLocation has said the class header should be in the Public folder, put the source file in the Private folder
	// 2) If InPath is contained within the Public or Classes folder of this module, place it in the equivalent path in the Private folder
	// 3) Otherwise, just place the source file at InPath (the default set above)
	if(AbsoluteInPath == RootPath)
	{
		OutSourcePath = (ClassPathLocation == EClassLocation::Public) ? PrivatePath : AbsoluteInPath;
	}
	else if(ClassPathLocation == EClassLocation::Public)
	{
		OutSourcePath = AbsoluteInPath.Replace(*PublicPath, *PrivatePath);
	}
	else if(ClassPathLocation == EClassLocation::Classes)
	{
		OutSourcePath = AbsoluteInPath.Replace(*ClassesPath, *PrivatePath);
	}

	return !OutHeaderPath.IsEmpty() && !OutSourcePath.IsEmpty();
}

bool GameProjectUtils::GetClassLocation(const FString& InPath, const FModuleContextInfo& ModuleInfo, EClassLocation& OutClassLocation, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /
	OutClassLocation = EClassLocation::UserDefined;

	if(!IsValidSourcePath(InPath, ModuleInfo, OutFailReason))
	{
		return false;
	}

	const FString RootPath = ModuleInfo.ModuleSourcePath;
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /
	const FString ClassesPath = RootPath / "Classes" / "";		// Ensure trailing /

	// If either the Public or Private path exists, and we're in the root, force the header/source file to use one of these folders
	const bool bPublicPathExists = IFileManager::Get().DirectoryExists(*PublicPath);
	const bool bPrivatePathExists = IFileManager::Get().DirectoryExists(*PrivatePath);
	const bool bForceInternalPath = AbsoluteInPath == RootPath && (bPublicPathExists || bPrivatePathExists);

	if(AbsoluteInPath == RootPath)
	{
		OutClassLocation = (bPublicPathExists || bForceInternalPath) ? EClassLocation::Public : EClassLocation::UserDefined;
	}
	else if(AbsoluteInPath.StartsWith(PublicPath))
	{
		OutClassLocation = EClassLocation::Public;
	}
	else if(AbsoluteInPath.StartsWith(PrivatePath))
	{
		OutClassLocation = EClassLocation::Private;
	}
	else if(AbsoluteInPath.StartsWith(ClassesPath))
	{
		OutClassLocation = EClassLocation::Classes;
	}
	else
	{
		OutClassLocation = EClassLocation::UserDefined;
	}

	return true;
}

GameProjectUtils::EProjectDuplicateResult GameProjectUtils::DuplicateProjectForUpgrade( const FString& InProjectFile, FString& OutNewProjectFile )
{
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Get the directory part of the project name
	FString OldDirectoryName = FPaths::GetPath(InProjectFile);
	FPaths::NormalizeDirectoryName(OldDirectoryName);
	FString NewDirectoryName = OldDirectoryName;

	// Strip off any previous version number from the project name
	for(int32 LastSpace; NewDirectoryName.FindLastChar(' ', LastSpace); )
	{
		const TCHAR *End = *NewDirectoryName + LastSpace + 1;
		if(End[0] != '4' || End[1] != '.' || !FChar::IsDigit(End[2]))
		{
			break;
		}

		End += 3;

		while(FChar::IsDigit(*End))
		{
			End++;
		}

		if(*End != 0)
		{
			break;
		}

		NewDirectoryName = NewDirectoryName.Left(LastSpace).TrimEnd();
	}

	// Append the new version number
	NewDirectoryName += FString::Printf(TEXT(" %s"), *FEngineVersion::Current().ToString(EVersionComponent::Minor));

	// Find a directory name that doesn't exist
	FString BaseDirectoryName = NewDirectoryName;
	for(int32 Idx = 2; IFileManager::Get().DirectoryExists(*NewDirectoryName); Idx++)
	{
		NewDirectoryName = FString::Printf(TEXT("%s - %d"), *BaseDirectoryName, Idx);
	}

	// Recursively find all the files we need to copy, excluding those that are within the directories listed in SourceDirectoriesToSkip
	struct FGatherFilesToCopyHelper
	{
	public:
		FGatherFilesToCopyHelper(FString InRootSourceDirectory)
			: RootSourceDirectory(MoveTemp(InRootSourceDirectory))
		{
			static const FString RelativeDirectoriesToSkip[] = {
				TEXT("Binaries"),
				TEXT("DerivedDataCache"),
				TEXT("Intermediate"),
				TEXT("Saved/Autosaves"),
				TEXT("Saved/Backup"),
				TEXT("Saved/Config"),
				TEXT("Saved/Cooked"),
				TEXT("Saved/HardwareSurvey"),
				TEXT("Saved/Logs"),
				TEXT("Saved/StagedBuilds"),
			};

			SourceDirectoriesToSkip.Reserve(ARRAY_COUNT(RelativeDirectoriesToSkip));
			for (const FString& RelativeDirectoryToSkip : RelativeDirectoriesToSkip)
			{
				SourceDirectoriesToSkip.Emplace(RootSourceDirectory / RelativeDirectoryToSkip);
			}
		}

		void GatherFilesToCopy(TArray<FString>& OutSourceDirectories, TArray<FString>& OutSourceFiles)
		{
			GatherFilesToCopy(RootSourceDirectory, OutSourceDirectories, OutSourceFiles);
		}

	private:
		void GatherFilesToCopy(const FString& InSourceDirectoryPath, TArray<FString>& OutSourceDirectories, TArray<FString>& OutSourceFiles)
		{
			const FString SourceDirectorySearchWildcard = InSourceDirectoryPath / TEXT("*");

			OutSourceDirectories.Emplace(InSourceDirectoryPath);

			TArray<FString> SourceFilenames;
			IFileManager::Get().FindFiles(SourceFilenames, *SourceDirectorySearchWildcard, true, false);

			OutSourceFiles.Reserve(OutSourceFiles.Num() + SourceFilenames.Num());
			for (const FString& SourceFilename : SourceFilenames)
			{
				OutSourceFiles.Emplace(InSourceDirectoryPath / SourceFilename);
			}

			TArray<FString> SourceSubDirectoryNames;
			IFileManager::Get().FindFiles(SourceSubDirectoryNames, *SourceDirectorySearchWildcard, false, true);

			for (const FString& SourceSubDirectoryName : SourceSubDirectoryNames)
			{
				const FString SourceSubDirectoryPath = InSourceDirectoryPath / SourceSubDirectoryName;
				if (!SourceDirectoriesToSkip.Contains(SourceSubDirectoryPath))
				{
					GatherFilesToCopy(SourceSubDirectoryPath, OutSourceDirectories, OutSourceFiles);
				}
			}
		}

		FString RootSourceDirectory;
		TArray<FString> SourceDirectoriesToSkip;
	};

	TArray<FString> SourceDirectories;
	TArray<FString> SourceFiles;
	FGatherFilesToCopyHelper(OldDirectoryName).GatherFilesToCopy(SourceDirectories, SourceFiles);

	// Copy everything
	bool bCopySucceeded = true;
	bool bUserCanceled = false;
	GWarn->BeginSlowTask(LOCTEXT("CreatingCopyOfProject", "Creating copy of project..."), true, true);
	for(int32 Idx = 0; Idx < SourceDirectories.Num() && bCopySucceeded; Idx++)
	{
		FString TargetDirectory = NewDirectoryName + SourceDirectories[Idx].Mid(OldDirectoryName.Len());
		bUserCanceled = GWarn->ReceivedUserCancel();
		bCopySucceeded = !bUserCanceled && PlatformFile.CreateDirectory(*TargetDirectory);
		GWarn->UpdateProgress(Idx + 1, SourceDirectories.Num() + SourceFiles.Num());
	}
	for(int32 Idx = 0; Idx < SourceFiles.Num() && bCopySucceeded; Idx++)
	{
		FString TargetFile = NewDirectoryName + SourceFiles[Idx].Mid(OldDirectoryName.Len());
		bUserCanceled = GWarn->ReceivedUserCancel();
		bCopySucceeded =  !bUserCanceled && PlatformFile.CopyFile(*TargetFile, *SourceFiles[Idx]);
		GWarn->UpdateProgress(SourceDirectories.Num() + Idx + 1, SourceDirectories.Num() + SourceFiles.Num());
	}
	GWarn->EndSlowTask();

	// Wipe the directory if the user canceled or we couldn't update
	if(!bCopySucceeded)
	{
		PlatformFile.DeleteDirectoryRecursively(*NewDirectoryName);
		if(bUserCanceled)
		{
			return EProjectDuplicateResult::UserCanceled;
		}
		else
		{
			return EProjectDuplicateResult::Failed;
		}
	}

	// Otherwise fixup the output project filename
	OutNewProjectFile = NewDirectoryName / FPaths::GetCleanFilename(InProjectFile);
	return EProjectDuplicateResult::Succeeded;
}

void GameProjectUtils::UpdateSupportedTargetPlatforms(const FName& InPlatformName, const bool bIsSupported)
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	if(!ProjectFilename.IsEmpty())
	{
		// First attempt to check out the file if SCC is enabled
		if(ISourceControlModule::Get().IsEnabled())
		{
			FText UnusedFailReason;
			CheckoutGameProjectFile(ProjectFilename, UnusedFailReason);
		}

		// Second make sure the file is writable
		if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFilename))
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFilename, false);
		}

		IProjectManager::Get().UpdateSupportedTargetPlatformsForCurrentProject(InPlatformName, bIsSupported);
	}
}

void GameProjectUtils::ClearSupportedTargetPlatforms()
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	if(!ProjectFilename.IsEmpty())
	{
		// First attempt to check out the file if SCC is enabled
		if(ISourceControlModule::Get().IsEnabled())
		{
			FText UnusedFailReason;
			CheckoutGameProjectFile(ProjectFilename, UnusedFailReason);
		}

		// Second make sure the file is writable
		if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFilename))
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFilename, false);
		}

		IProjectManager::Get().ClearSupportedTargetPlatformsForCurrentProject();
	}
}

void GameProjectUtils::UpdateAdditionalPluginDirectory(const FString& InDir, const bool bAddOrRemove)
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	if (!ProjectFilename.IsEmpty())
	{
		// First attempt to check out the file if SCC is enabled
		if (ISourceControlModule::Get().IsEnabled())
		{
			FText UnusedFailReason;
			CheckoutGameProjectFile(ProjectFilename, UnusedFailReason);
		}

		// Second make sure the file is writable
		if (FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFilename))
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFilename, false);
		}

		IProjectManager::Get().UpdateAdditionalPluginDirectory(InDir, bAddOrRemove);
	}
}

bool GameProjectUtils::ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason)
{
	const FString FullFileName = FPaths::EngineContentDir() / TEXT("Editor") / TEXT("Templates") / TemplateFileName;
	if ( FFileHelper::LoadFileToString(OutFileContents, *FullFileName) )
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("FullFileName"), FText::FromString( FullFileName ) );
	OutFailReason = FText::Format( LOCTEXT("FailedToReadTemplateFile", "Failed to read template file \"{FullFileName}\""), Args );
	return false;
}

bool GameProjectUtils::WriteOutputFile(const FString& OutputFilename, const FString& OutputFileContents, FText& OutFailReason)
{
	if ( FFileHelper::SaveStringToFile(OutputFileContents, *OutputFilename ) )
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("OutputFilename"), FText::FromString( OutputFilename ) );
	OutFailReason = FText::Format( LOCTEXT("FailedToWriteOutputFile", "Failed to write output file \"{OutputFilename}\". Perhaps the file is Read-Only?"), Args );
	return false;
}

FString GameProjectUtils::MakeCopyrightLine()
{
	const FString CopyrightNotice = GetDefault<UGeneralProjectSettings>()->CopyrightNotice;
	if (!CopyrightNotice.IsEmpty())
	{
		return FString(TEXT("// ")) + CopyrightNotice;
	}
	else
	{
		return FString();
	}
}

FString GameProjectUtils::MakeCommaDelimitedList(const TArray<FString>& InList, bool bPlaceQuotesAroundEveryElement)
{
	FString ReturnString;

	for ( auto ListIt = InList.CreateConstIterator(); ListIt; ++ListIt )
	{
		FString ElementStr;
		if ( bPlaceQuotesAroundEveryElement )
		{
			ElementStr = FString::Printf( TEXT("\"%s\""), **ListIt);
		}
		else
		{
			ElementStr = *ListIt;
		}

		if ( ReturnString.Len() > 0 )
		{
			// If this is not the first item in the list, prepend with a comma
			ElementStr = FString::Printf(TEXT(", %s"), *ElementStr);
		}

		ReturnString += ElementStr;
	}

	return ReturnString;
}

FString GameProjectUtils::MakeIncludeList(const TArray<FString>& InList)
{
	FString ReturnString;

	for ( auto ListIt = InList.CreateConstIterator(); ListIt; ++ListIt )
	{
		ReturnString += FString::Printf( TEXT("#include \"%s\"") LINE_TERMINATOR, **ListIt);
	}

	return ReturnString;
}

FString GameProjectUtils::DetermineModuleIncludePath(const FModuleContextInfo& ModuleInfo, const FString& FileRelativeTo)
{
	FString ModuleIncludePath;

	if(FindSourceFileInProject(ModuleInfo.ModuleName + ".h", ModuleInfo.ModuleSourcePath, ModuleIncludePath))
	{
		// Work out where the module header is; 
		// if it's Public then we can include it without any path since all Public and Classes folders are on the include path
		// if it's located elsewhere, then we'll need to include it relative to the module source root as we can't guarantee 
		// that other folders are on the include paths
		EClassLocation ModuleLocation;
		if(GetClassLocation(ModuleIncludePath, ModuleInfo, ModuleLocation))
		{
			if(ModuleLocation == EClassLocation::Public || ModuleLocation == EClassLocation::Classes)
			{
				ModuleIncludePath = ModuleInfo.ModuleName + ".h";
			}
			else
			{
				// If the path to our new class is the same as the path to the module, we can include it directly
				const FString ModulePath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(ModuleIncludePath));
				const FString ClassPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FileRelativeTo));
				if(ModulePath == ClassPath)
				{
					ModuleIncludePath = ModuleInfo.ModuleName + ".h";
				}
				else
				{
					// Updates ModuleIncludePath internally
					if(!FPaths::MakePathRelativeTo(ModuleIncludePath, *ModuleInfo.ModuleSourcePath))
					{
						// Failed; just assume we can include it without any relative path
						ModuleIncludePath = ModuleInfo.ModuleName + ".h";
					}
				}
			}
		}
		else
		{
			// Failed; just assume we can include it without any relative path
			ModuleIncludePath = ModuleInfo.ModuleName + ".h";
		}
	}
	else
	{
		// This could potentially fail when generating new projects if the module file hasn't yet been created; just assume we can include it without any relative path
		ModuleIncludePath = ModuleInfo.ModuleName + ".h";
	}

	return ModuleIncludePath;
}

/**
 * Generates UObject class constructor definition with property overrides.
 *
 * @param Out String to assign generated constructor to.
 * @param PrefixedClassName Prefixed class name for which we generate the constructor.
 * @param PropertyOverridesStr String with property overrides in the constructor.
 * @param OutFailReason Template read function failure reason.
 *
 * @returns True on success. False otherwise.
 */
bool GenerateConstructorDefinition(FString& Out, const FString& PrefixedClassName, const FString& PropertyOverridesStr, FText& OutFailReason)
{
	FString Template;
	if (!GameProjectUtils::ReadTemplateFile(TEXT("UObjectClassConstructorDefinition.template"), Template, OutFailReason))
	{
		return false;
	}

	Out = Template.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	Out = Out.Replace(TEXT("%PROPERTY_OVERRIDES%"), *PropertyOverridesStr, ESearchCase::CaseSensitive);

	return true;
}

/**
 * Generates UObject class constructor declaration.
 *
 * @param Out String to assign generated constructor to.
 * @param PrefixedClassName Prefixed class name for which we generate the constructor.
 * @param OutFailReason Template read function failure reason.
 *
 * @returns True on success. False otherwise.
 */
bool GenerateConstructorDeclaration(FString& Out, const FString& PrefixedClassName, FText& OutFailReason)
{
	FString Template;
	if (!GameProjectUtils::ReadTemplateFile(TEXT("UObjectClassConstructorDeclaration.template"), Template, OutFailReason))
	{
		return false;
	}

	Out = Template.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);

	return true;
}

bool GameProjectUtils::GenerateClassHeaderFile(const FString& NewHeaderFileName, const FString UnPrefixedClassName, const FNewClassInfo ParentClassInfo, const TArray<FString>& ClassSpecifierList, const FString& ClassProperties, const FString& ClassFunctionDeclarations, FString& OutSyncLocation, const FModuleContextInfo& ModuleInfo, bool bDeclareConstructor, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(ParentClassInfo.GetHeaderTemplateFilename(), Template, OutFailReason) )
	{
		return false;
	}

	const FString ClassPrefix = ParentClassInfo.GetClassPrefixCPP();
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;
	const FString PrefixedBaseClassName = ClassPrefix + ParentClassInfo.GetClassNameCPP();

	FString BaseClassIncludeDirective;
	FString BaseClassIncludePath;
	if(ParentClassInfo.GetIncludePath(BaseClassIncludePath))
	{
		BaseClassIncludeDirective = FString::Printf(TEXT("#include \"%s\""), *BaseClassIncludePath);
	}

	FString ModuleAPIMacro;
	{
		EClassLocation ClassPathLocation = EClassLocation::UserDefined;
		if ( GetClassLocation(NewHeaderFileName, ModuleInfo, ClassPathLocation) )
		{
			// If this class isn't Private, make sure and include the API macro so it can be linked within other modules
			if ( ClassPathLocation != EClassLocation::Private )
			{
				ModuleAPIMacro = ModuleInfo.ModuleName.ToUpper() + "_API "; // include a trailing space for the template formatting
			}
		}
	}

	FString EventualConstructorDeclaration;
	if (bDeclareConstructor)
	{
		if (!GenerateConstructorDeclaration(EventualConstructorDeclaration, PrefixedClassName, OutFailReason))
		{
			return false;
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_MODULE_API_MACRO%"), *ModuleAPIMacro, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UCLASS_SPECIFIER_LIST%"), *MakeCommaDelimitedList(ClassSpecifierList, false), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_BASE_CLASS_NAME%"), *PrefixedBaseClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EVENTUAL_CONSTRUCTOR_DECLARATION%"), *EventualConstructorDeclaration, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_PROPERTIES%"), *ClassProperties, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_FUNCTION_DECLARATIONS%"), *ClassFunctionDeclarations, ESearchCase::CaseSensitive);
	if (BaseClassIncludeDirective.Len() == 0)
	{
		FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%") LINE_TERMINATOR, TEXT(""), ESearchCase::CaseSensitive);
	}
	FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%"), *BaseClassIncludeDirective, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation( FinalOutput, OutSyncLocation );

	return WriteOutputFile(NewHeaderFileName, FinalOutput, OutFailReason);
}

static bool TryParseIncludeDirective(const FString& Text, int StartPos, int EndPos, FString& IncludePath)
{
	// Check if the line starts with a # character
	int Pos = StartPos;
	while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
	{
		Pos++;
	}
	if (Pos == EndPos || Text[Pos++] != '#')
	{
		return false;
	}
	while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
	{
		Pos++;
	}

	// Check it's an include directive
	const TCHAR* IncludeText = TEXT("include");
	for (int Idx = 0; IncludeText[Idx] != 0; Idx++)
	{
		if (Pos == EndPos || Text[Pos] != IncludeText[Idx])
		{
			return false;
		}
		Pos++;
	}
	while (Pos < EndPos && FChar::IsWhitespace(Text[Pos]))
	{
		Pos++;
	}

	// Parse out the quoted include path
	if (Pos == EndPos || Text[Pos++] != '"')
	{
		return false;
	}
	int IncludePathPos = Pos;
	while (Pos < EndPos && Text[Pos] != '"')
	{
		Pos++;
	}
	IncludePath = Text.Mid(IncludePathPos, Pos - IncludePathPos);
	return true;
}

static bool IsUsingOldStylePch(FString BaseDir)
{
	// Find all the cpp files under the base directory
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *BaseDir, TEXT("*.cpp"), true, false, false);

	// Parse the first include directive for up to 16 include paths
	TArray<FString> FirstIncludedFiles;
	for (int Idx = 0; Idx < Files.Num() && Idx < 16; Idx++)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Files[Idx]);

		int LinePos = 0;
		while(LinePos < Text.Len())
		{
			int EndOfLinePos = LinePos;
			while (EndOfLinePos < Text.Len() && Text[EndOfLinePos] != '\n')
			{
				EndOfLinePos++;
			}

			FString IncludePath;
			if (TryParseIncludeDirective(Text, LinePos, EndOfLinePos, IncludePath))
			{
				FirstIncludedFiles.AddUnique(FPaths::GetCleanFilename(IncludePath));
				break;
			}

			LinePos = EndOfLinePos + 1;
		}
	}
	return FirstIncludedFiles.Num() == 1 && Files.Num() > 1;
}

bool GameProjectUtils::GenerateClassCPPFile(const FString& NewCPPFileName, const FString UnPrefixedClassName, const FNewClassInfo ParentClassInfo, const TArray<FString>& AdditionalIncludes, const TArray<FString>& PropertyOverrides, const FString& AdditionalMemberDefinitions, FString& OutSyncLocation, const FModuleContextInfo& ModuleInfo, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(ParentClassInfo.GetSourceTemplateFilename(), Template, OutFailReason) )
	{
		return false;
	}

	const FString ClassPrefix = ParentClassInfo.GetClassPrefixCPP();
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;
	const FString PrefixedBaseClassName = ClassPrefix + ParentClassInfo.GetClassNameCPP();

	EClassLocation ClassPathLocation = EClassLocation::UserDefined;
	if ( !GetClassLocation(NewCPPFileName, ModuleInfo, ClassPathLocation, &OutFailReason) )
	{
		return false;
	}

	FString AdditionalIncludesStr;
	for (int32 IncludeIdx = 0; IncludeIdx < AdditionalIncludes.Num(); ++IncludeIdx)
	{
		if (IncludeIdx > 0)
		{
			AdditionalIncludesStr += LINE_TERMINATOR;
		}

		AdditionalIncludesStr += FString::Printf(TEXT("#include \"%s\""), *AdditionalIncludes[IncludeIdx]);
	}

	FString PropertyOverridesStr;
	for ( int32 OverrideIdx = 0; OverrideIdx < PropertyOverrides.Num(); ++OverrideIdx )
	{
		if ( OverrideIdx > 0 )
		{
			PropertyOverridesStr += LINE_TERMINATOR;
		}

		PropertyOverridesStr += TEXT("\t");
		PropertyOverridesStr += *PropertyOverrides[OverrideIdx];
	}

	// Calculate the correct include path for the module header
	FString PchIncludeDirective;
	if (IsUsingOldStylePch(ModuleInfo.ModuleSourcePath))
	{
		const FString ModuleIncludePath = DetermineModuleIncludePath(ModuleInfo, NewCPPFileName);
		if (ModuleIncludePath.Len() > 0)
		{
			PchIncludeDirective = FString::Printf(TEXT("#include \"%s\""), *ModuleIncludePath);
		}
	}

	FString EventualConstructorDefinition;
	if (PropertyOverrides.Num() != 0)
	{
		if (!GenerateConstructorDefinition(EventualConstructorDefinition, PrefixedClassName, PropertyOverridesStr, OutFailReason))
		{
			return false;
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleInfo.ModuleName, ESearchCase::CaseSensitive);
	if (PchIncludeDirective.Len() == 0)
	{
		FinalOutput = FinalOutput.Replace(TEXT("%PCH_INCLUDE_DIRECTIVE%") LINE_TERMINATOR, TEXT(""), ESearchCase::CaseSensitive);
	}
	FinalOutput = FinalOutput.Replace(TEXT("%PCH_INCLUDE_DIRECTIVE%"), *PchIncludeDirective, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EVENTUAL_CONSTRUCTOR_DEFINITION%"), *EventualConstructorDefinition, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ADDITIONAL_MEMBER_DEFINITIONS%"), *AdditionalMemberDefinitions, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ADDITIONAL_INCLUDE_DIRECTIVES%"), *AdditionalIncludesStr, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation( FinalOutput, OutSyncLocation );

	return WriteOutputFile(NewCPPFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleBuildFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& PublicDependencyModuleNames, const TArray<FString>& PrivateDependencyModuleNames, FText& OutFailReason)
{
	FString Template;
	if (!ReadTemplateFile(TEXT("GameModule.Build.cs.template"), Template, OutFailReason))
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GeneratePluginModuleBuildFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& PublicDependencyModuleNames, const TArray<FString>& PrivateDependencyModuleNames, FText& OutFailReason, bool bUseExplicitOrSharedPCHs/* = true*/)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("PluginModule.Build.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	
	const FString PCHUsage = bUseExplicitOrSharedPCHs ? TEXT("UseExplicitOrSharedPCHs") : TEXT("UseSharedPCHs");
	FinalOutput = FinalOutput.Replace(TEXT("%PCH_USAGE%"), *PCHUsage, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleTargetFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& ExtraModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("Stub.Target.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EXTRA_MODULE_NAMES%"), *MakeCommaDelimitedList(ExtraModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%TARGET_TYPE%"), TEXT("Game"), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateEditorModuleBuildFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& PublicDependencyModuleNames, const TArray<FString>& PrivateDependencyModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("EditorModule.Build.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateEditorModuleTargetFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& ExtraModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("Stub.Target.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EXTRA_MODULE_NAMES%"), *MakeCommaDelimitedList(ExtraModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%TARGET_TYPE%"), TEXT("Editor"), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleCPPFile(const FString& NewBuildFileName, const FString& ModuleName, const FString& GameName, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("GameModule.cpp.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%GAME_NAME%"), *GameName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleHeaderFile(const FString& NewBuildFileName, const TArray<FString>& PublicHeaderIncludes, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("GameModule.h.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_HEADER_INCLUDES%"), *MakeIncludeList(PublicHeaderIncludes), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GeneratePluginModuleCPPFile(const FString& CPPFileName, const FString& ModuleName, const FString& StartupSourceCode, FText& OutFailReason)
{
	FString Template;
	if (!ReadTemplateFile(TEXT("PluginModule.cpp.template"), Template, OutFailReason))
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_STARTUP_CODE%"), *StartupSourceCode, ESearchCase::CaseSensitive);

	return WriteOutputFile(CPPFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GeneratePluginModuleHeaderFile(const FString& HeaderFileName, const TArray<FString>& PublicHeaderIncludes, FText& OutFailReason)
{
	FString Template;
	if (!ReadTemplateFile(TEXT("PluginModule.h.template"), Template, OutFailReason))
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_HEADER_INCLUDES%"), *MakeIncludeList(PublicHeaderIncludes), ESearchCase::CaseSensitive);

	return WriteOutputFile(HeaderFileName, FinalOutput, OutFailReason);
}

void GameProjectUtils::OnUpdateProjectConfirm()
{
	UpdateProject();
}

void GameProjectUtils::UpdateProject(const FProjectDescriptorModifier& Modifier)
{
	UpdateProject_Impl(&Modifier);
}

void GameProjectUtils::UpdateProject()
{
	UpdateProject_Impl(nullptr);
}

void GameProjectUtils::UpdateProject_Impl(const FProjectDescriptorModifier* Modifier)
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	const FString& ShortFilename = FPaths::GetCleanFilename(ProjectFilename);
	FText FailReason;
	FText UpdateMessage;
	SNotificationItem::ECompletionState NewCompletionState;
	if (UpdateGameProjectFile_Impl(ProjectFilename, FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier(), Modifier, FailReason))
	{
		// The project was updated successfully.
		FFormatNamedArguments Args;
		Args.Add( TEXT("ShortFilename"), FText::FromString( ShortFilename ) );
		UpdateMessage = FText::Format( LOCTEXT("ProjectFileUpdateComplete", "{ShortFilename} was successfully updated."), Args );
		NewCompletionState = SNotificationItem::CS_Success;
	}
	else
	{
		// The user chose to update, but the update failed. Notify the user.
		FFormatNamedArguments Args;
		Args.Add( TEXT("ShortFilename"), FText::FromString( ShortFilename ) );
		Args.Add( TEXT("FailReason"), FailReason );
		UpdateMessage = FText::Format( LOCTEXT("ProjectFileUpdateFailed", "{ShortFilename} failed to update. {FailReason}"), Args );
		NewCompletionState = SNotificationItem::CS_Fail;
	}

	if ( UpdateGameProjectNotification.IsValid() )
	{
		UpdateGameProjectNotification.Pin()->SetCompletionState(NewCompletionState);
		UpdateGameProjectNotification.Pin()->SetText(UpdateMessage);
		UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
		UpdateGameProjectNotification.Reset();
	}
}

void GameProjectUtils::UpdateProject(const TArray<FString>* StartupModuleNames)
{
	UpdateProject(
		FProjectDescriptorModifier::CreateLambda(
			[StartupModuleNames](FProjectDescriptor& Desc)
			{
				if (StartupModuleNames != nullptr)
				{
					return UpdateStartupModuleNames(Desc, StartupModuleNames);
				}

				return false;
			}));
}

void GameProjectUtils::OnUpdateProjectCancel()
{
	if ( UpdateGameProjectNotification.IsValid() )
	{
		UpdateGameProjectNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
		UpdateGameProjectNotification.Reset();
	}
}

void GameProjectUtils::TryMakeProjectFileWriteable(const FString& ProjectFile)
{
	// First attempt to check out the file if SCC is enabled
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		FText FailReason;
		GameProjectUtils::CheckoutGameProjectFile(ProjectFile, FailReason);
	}

	// Check if it's writable
	if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFile))
	{
		FText ShouldMakeProjectWriteable = LOCTEXT("ShouldMakeProjectWriteable_Message", "'{ProjectFilename}' is read-only and cannot be updated. Would you like to make it writeable?");

		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ProjectFilename"), FText::FromString(ProjectFile));

		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldMakeProjectWriteable, Arguments)) == EAppReturnType::Yes)
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFile, false);
		}
	}
}

bool GameProjectUtils::UpdateGameProjectFile(const FString& ProjectFile, const FString& EngineIdentifier, const FProjectDescriptorModifier& Modifier, FText& OutFailReason)
{
	return UpdateGameProjectFile_Impl(ProjectFile, EngineIdentifier, &Modifier, OutFailReason);
}

bool GameProjectUtils::UpdateGameProjectFile(const FString& ProjectFile, const FString& EngineIdentifier, FText& OutFailReason)
{
	return UpdateGameProjectFile_Impl(ProjectFile, EngineIdentifier, nullptr, OutFailReason);
}

bool GameProjectUtils::UpdateGameProjectFile_Impl(const FString& ProjectFile, const FString& EngineIdentifier, const FProjectDescriptorModifier* Modifier, FText& OutFailReason)
{
	// Make sure we can write to the project file
	TryMakeProjectFileWriteable(ProjectFile);

	// Load the descriptor
	FProjectDescriptor Descriptor;
	if(Descriptor.Load(ProjectFile, OutFailReason))
	{
		if (Modifier && Modifier->IsBound() && !Modifier->Execute(Descriptor))
		{
			// If modifier returns false it means that we want to drop changes.
			return true;
		}

		// Update file on disk
		return Descriptor.Save(ProjectFile, OutFailReason) && FDesktopPlatformModule::Get()->SetEngineIdentifierForProject(ProjectFile, EngineIdentifier);
	}
	return false;
}

bool GameProjectUtils::UpdateGameProjectFile(const FString& ProjectFilename, const FString& EngineIdentifier, const TArray<FString>* StartupModuleNames, FText& OutFailReason)
{
	return UpdateGameProjectFile(ProjectFilename, EngineIdentifier,
		FProjectDescriptorModifier::CreateLambda(
			[StartupModuleNames](FProjectDescriptor& Desc)
			{
				if (StartupModuleNames != nullptr)
				{
					return UpdateStartupModuleNames(Desc, StartupModuleNames);
				}

				return false;
			}
		), OutFailReason);
}

bool GameProjectUtils::CheckoutGameProjectFile(const FString& ProjectFilename, FText& OutFailReason)
{
	if ( !ensure(ProjectFilename.Len()) )
	{
		OutFailReason = LOCTEXT("NoProjectFilename", "The project filename was not specified.");
		return false;
	}

	if ( !ISourceControlModule::Get().IsEnabled() )
	{
		OutFailReason = LOCTEXT("SCCDisabled", "Source control is not enabled. Enable source control in the preferences menu.");
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(ProjectFilename);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(AbsoluteFilename);

	bool bSuccessfullyCheckedOut = false;
	OutFailReason = LOCTEXT("SCCStateInvalid", "Could not determine source control state.");

	if(SourceControlState.IsValid())
	{
		if(SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() || !SourceControlState->IsSourceControlled())
		{
			// Already checked out or opened for add... or not in the depot at all
			bSuccessfullyCheckedOut = true;
		}
		else if(SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutFailReason = LOCTEXT("SCCCheckoutFailed", "Failed to check out the project file.");
			}
		}
		else if(!SourceControlState->IsCurrent())
		{
			OutFailReason = LOCTEXT("SCCNotCurrent", "The project file is not at head revision.");
		}
	}

	return bSuccessfullyCheckedOut;
}

FString GameProjectUtils::GetDefaultProjectTemplateFilename()
{
	return TEXT("");
}

void FindCodeFiles(const TCHAR* BaseDirectory, TArray<FString>& FileNames, int32 MaxNumFileNames)
{
	struct FDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FileNames;
		int32 MaxNumFileNames;

		FDirectoryVisitor(TArray<FString>& InFileNames, int32 InMaxNumFileNames) 
			: FileNames(InFileNames)
			, MaxNumFileNames(InMaxNumFileNames)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if(bIsDirectory)
			{
				FString CleanDirectoryName(FPaths::GetCleanFilename(FilenameOrDirectory));
				if(!CleanDirectoryName.StartsWith(TEXT(".")))
				{
					FindCodeFiles(FilenameOrDirectory, FileNames, MaxNumFileNames);
				}
			}
			else
			{
				FString FileName(FilenameOrDirectory);
				if(FileName.EndsWith(TEXT(".h")) || FileName.EndsWith(".cpp"))
				{
					FileNames.Add(FileName);
				}
			}
			return (FileNames.Num() < MaxNumFileNames);
		}
	};

	// Enumerate the contents of the current directory
	FDirectoryVisitor Visitor(FileNames, MaxNumFileNames);
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(BaseDirectory, Visitor);
}

void GameProjectUtils::GetProjectCodeFilenames(TArray<FString>& OutProjectCodeFilenames)
{
	FindCodeFiles(*FPaths::GameSourceDir(), OutProjectCodeFilenames, INT_MAX);
}

int32 GameProjectUtils::GetProjectCodeFileCount()
{
	TArray<FString> Filenames;
	GetProjectCodeFilenames(Filenames);
	return Filenames.Num();
}

void GameProjectUtils::GetProjectSourceDirectoryInfo(int32& OutNumCodeFiles, int64& OutDirectorySize)
{
	TArray<FString> Filenames;
	GetProjectCodeFilenames(Filenames);
	OutNumCodeFiles = Filenames.Num();

	OutDirectorySize = 0;
	for (const auto& filename : Filenames)
	{
		OutDirectorySize += IFileManager::Get().FileSize(*filename);
	}
}

bool GameProjectUtils::ProjectHasCodeFiles()
{
	TArray<FString> FileNames;
	FindCodeFiles(*FPaths::GameSourceDir(), FileNames, 1);
	return FileNames.Num() > 0;
}

static bool RequiresBuild()
{
	// determine if there are any project icons
	FString IconDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Build/IOS/Resources/Graphics"));
	struct FDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FileNames;

		FDirectoryVisitor(TArray<FString>& InFileNames)
			: FileNames(InFileNames)
		{
		}

		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			FString FileName(FilenameOrDirectory);
			if (FileName.EndsWith(TEXT(".png")) && FileName.Contains(TEXT("Icon")))
			{
				FileNames.Add(FileName);
			}
			return true;
		}
	};

	// Enumerate the contents of the current directory
	TArray<FString> FileNames;
	FDirectoryVisitor Visitor(FileNames);
	FPlatformFileManager::Get().GetPlatformFile().IterateDirectory(*IconDir, Visitor);

	if (FileNames.Num() > 0)
	{
		return true;
	}
	return false;
}

static bool PlatformRequiresBuild(const FName InPlatformInfoName)
{
	const PlatformInfo::FPlatformInfo* const PlatInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatInfo);

	if (PlatInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::Installed)
	{
		const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatInfo->TargetPlatformName.ToString());
		if (Platform)
		{
			if (InPlatformInfoName.ToString() == TEXT("IOS"))
			{
				return RequiresBuild();
			}
		}
	}
	return false;
}

bool GameProjectUtils::ProjectRequiresBuild(const FName InPlatformInfoName)
{
	//  early out on projects with code files
	if (ProjectHasCodeFiles())
	{
		return true;
	}

	bool bRequiresBuild = false;

	if (!FApp::IsEngineInstalled())
	{
		// check to see if the default build settings have changed
		bRequiresBuild |= !HasDefaultBuildSettings(InPlatformInfoName);
	}
	else
	{
		// check to see if the platform rules we need a build
		bRequiresBuild |= PlatformRequiresBuild(InPlatformInfoName);
	}

	// check to see if any plugins beyond the defaults have been enabled
	bRequiresBuild |= IProjectManager::Get().IsNonDefaultPluginEnabled();

	// check to see if Blueprint nativization is enabled in the Project settings
	bRequiresBuild |= GetDefault<UProjectPackagingSettings>()->BlueprintNativizationMethod != EProjectPackagingBlueprintNativizationMethod::Disabled;

	return bRequiresBuild;
}

bool GameProjectUtils::DoProjectSettingsMatchDefault(const FString& InPlatformName, const FString& InSection, const TArray<FString>* InBoolKeys, const TArray<FString>* InIntKeys, const TArray<FString>* InStringKeys)
{
	FConfigFile ProjIni;
	FConfigFile DefaultIni;
	FConfigCacheIni::LoadLocalIniFile(ProjIni, TEXT("Engine"), true, *InPlatformName, true);
	FConfigCacheIni::LoadExternalIniFile(DefaultIni, TEXT("Engine"), *FPaths::EngineConfigDir(), *FPaths::EngineConfigDir(), true, NULL, true);

	if (InBoolKeys != NULL)
	{
		for (int Index = 0; Index < InBoolKeys->Num(); ++Index)
		{
			FString Default(TEXT("False")), Project(TEXT("False"));
			DefaultIni.GetString(*InSection, *((*InBoolKeys)[Index]), Default);
			ProjIni.GetString(*InSection, *((*InBoolKeys)[Index]), Project);
			if (Default.Compare(Project, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
	}

	if (InIntKeys != NULL)
	{
		for (int Index = 0; Index < InIntKeys->Num(); ++Index)
		{
			int64 Default(0), Project(0);
			DefaultIni.GetInt64(*InSection, *((*InIntKeys)[Index]), Default);
			ProjIni.GetInt64(*InSection, *((*InIntKeys)[Index]), Project);
			if (Default != Project)
			{
				return false;
			}
		}
	}

	if (InStringKeys != NULL)
	{
		for (int Index = 0; Index < InStringKeys->Num(); ++Index)
		{
			FString Default(TEXT("False")), Project(TEXT("False"));
			DefaultIni.GetString(*InSection, *((*InStringKeys)[Index]), Default);
			ProjIni.GetString(*InSection, *((*InStringKeys)[Index]), Project);
			if (Default.Compare(Project, ESearchCase::IgnoreCase))
			{
				return false;
			}
		}
	}

	return true;
}

bool GameProjectUtils::HasDefaultBuildSettings(const FName InPlatformInfoName)
{
	// first check default build settings for all platforms
	TArray<FString> BoolKeys, IntKeys, StringKeys, BuildKeys;
	BuildKeys.Add(TEXT("bCompileApex")); BuildKeys.Add(TEXT("bCompileICU"));
	BuildKeys.Add(TEXT("bCompileSimplygon")); BuildKeys.Add(TEXT("bCompileSimplygonSSF")); BuildKeys.Add(TEXT("bCompileLeanAndMeanUE"));
	BuildKeys.Add(TEXT("bIncludeADO"));	BuildKeys.Add(TEXT("bCompileRecast")); BuildKeys.Add(TEXT("bCompileSpeedTree"));
	BuildKeys.Add(TEXT("bCompileWithPluginSupport")); BuildKeys.Add(TEXT("bCompilePhysXVehicle")); BuildKeys.Add(TEXT("bCompileFreeType"));
	BuildKeys.Add(TEXT("bCompileForSize"));	BuildKeys.Add(TEXT("bCompileCEF3"));

	const PlatformInfo::FPlatformInfo* const PlatInfo = PlatformInfo::FindPlatformInfo(InPlatformInfoName);
	check(PlatInfo);

	if (!DoProjectSettingsMatchDefault(PlatInfo->TargetPlatformName.ToString(), TEXT("/Script/BuildSettings.BuildSettings"), &BuildKeys))
	{
		return false;
	}

	if (PlatInfo->SDKStatus == PlatformInfo::EPlatformSDKStatus::Installed)
	{
		const ITargetPlatform* const Platform = GetTargetPlatformManager()->FindTargetPlatform(PlatInfo->TargetPlatformName.ToString());
		if (Platform)
		{
			FString PlatformSection;
			Platform->GetBuildProjectSettingKeys(PlatformSection, BoolKeys, IntKeys, StringKeys);
			bool bMatchDefault = DoProjectSettingsMatchDefault(PlatInfo->TargetPlatformName.ToString(), PlatformSection, &BoolKeys, &IntKeys, &StringKeys);
			if (bMatchDefault)
			{
				if (InPlatformInfoName.ToString() == TEXT("IOS"))
				{
					return !RequiresBuild();
				}
			}
		}
	}
	return true;
}

TArray<FString> GameProjectUtils::GetRequiredAdditionalDependencies(const FNewClassInfo& ClassInfo)
{
	TArray<FString> Out;

	switch (ClassInfo.ClassType)
	{
	case FNewClassInfo::EClassType::SlateWidget:
	case FNewClassInfo::EClassType::SlateWidgetStyle:
		Out.Reserve(2);
		Out.Add(TEXT("Slate"));
		Out.Add(TEXT("SlateCore"));
		break;

	case FNewClassInfo::EClassType::UObject:
		auto ClassPackageName = ClassInfo.BaseClass->GetOutermost()->GetFName().ToString();

		checkf(ClassPackageName.StartsWith(TEXT("/Script/")), TEXT("Class outermost should start with /Script/"));

		Out.Add(ClassPackageName.Mid(8)); // Skip the /Script/ prefix.
		break;
	}

	return Out;
}

GameProjectUtils::EAddCodeToProjectResult GameProjectUtils::AddCodeToProject_Internal(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason)
{
	if ( !ParentClassInfo.IsSet() )
	{
		OutFailReason = LOCTEXT("MissingParentClass", "You must specify a parent class");
		return EAddCodeToProjectResult::InvalidInput;
	}

	const FString CleanClassName = ParentClassInfo.GetCleanClassName(NewClassName);
	const FString FinalClassName = ParentClassInfo.GetFinalClassName(NewClassName);

	if (!IsValidClassNameForCreation(FinalClassName, ModuleInfo, DisallowedHeaderNames, OutFailReason))
	{
		return EAddCodeToProjectResult::InvalidInput;
	}

	if ( !FApp::HasProjectName() )
	{
		OutFailReason = LOCTEXT("AddCodeToProject_NoGameName", "You can not add code because you have not loaded a project.");
		return EAddCodeToProjectResult::FailedToAddCode;
	}

	FString NewHeaderPath;
	FString NewCppPath;
	if ( !CalculateSourcePaths(NewClassPath, ModuleInfo, NewHeaderPath, NewCppPath, &OutFailReason) )
	{
		return EAddCodeToProjectResult::FailedToAddCode;
	}

	FScopedSlowTask SlowTask( 7, LOCTEXT( "AddingCodeToProject", "Adding code to project..." ) );
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame();

	auto RequiredDependencies = GetRequiredAdditionalDependencies(ParentClassInfo);
	RequiredDependencies.Remove(ModuleInfo.ModuleName);

	// Update project file if needed.
	auto bUpdateProjectModules = false;
	
	// If the project does not already contain code, add the primary game module
	TArray<FString> CreatedFiles;
	TArray<FString> StartupModuleNames;

	const bool bProjectHadCodeFiles = ProjectHasCodeFiles();
	if (!bProjectHadCodeFiles)
	{
		// We always add the basic source code to the root directory, not the potential sub-directory provided by NewClassPath
		const FString SourceDir = FPaths::GameSourceDir().LeftChop(1); // Trim the trailing /

		// Assuming the game name is the same as the primary game module name
		const FString GameModuleName = FApp::GetProjectName();

		if ( GenerateBasicSourceCode(SourceDir, GameModuleName, FPaths::ProjectDir(), StartupModuleNames, CreatedFiles, OutFailReason) )
		{
			bUpdateProjectModules = true;
		}
		else
		{
			DeleteCreatedFiles(SourceDir, CreatedFiles);
			return EAddCodeToProjectResult::FailedToAddCode;
		}
	}

	if (RequiredDependencies.Num() > 0 || bUpdateProjectModules)
	{
		UpdateProject(
			FProjectDescriptorModifier::CreateLambda(
			[&StartupModuleNames, &RequiredDependencies, &ModuleInfo, bUpdateProjectModules](FProjectDescriptor& Descriptor)
			{
				bool bNeedsUpdate = false;

				bNeedsUpdate |= UpdateStartupModuleNames(Descriptor, bUpdateProjectModules ? &StartupModuleNames : nullptr);
				bNeedsUpdate |= UpdateRequiredAdditionalDependencies(Descriptor, RequiredDependencies, ModuleInfo.ModuleName);

				return bNeedsUpdate;
			}));
	}

	SlowTask.EnterProgressFrame();

	// Class Header File
	const FString NewHeaderFilename = NewHeaderPath / ParentClassInfo.GetHeaderFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		TArray<FString> ClassSpecifiers;

		// Set UCLASS() specifiers based on parent class type. Currently, only UInterface uses this.
		if (ParentClassInfo.ClassType == FNewClassInfo::EClassType::UInterface)
		{
			ClassSpecifiers.Add(TEXT("MinimalAPI"));
		}

		if ( GenerateClassHeaderFile(NewHeaderFilename, CleanClassName, ParentClassInfo, ClassSpecifiers, TEXT(""), TEXT(""), UnusedSyncLocation, ModuleInfo, false, OutFailReason) )
		{
			CreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			DeleteCreatedFiles(NewHeaderPath, CreatedFiles);
			return EAddCodeToProjectResult::FailedToAddCode;
		}
	}

	SlowTask.EnterProgressFrame();

	// Class CPP file
	const FString NewCppFilename = NewCppPath / ParentClassInfo.GetSourceFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		if ( GenerateClassCPPFile(NewCppFilename, CleanClassName, ParentClassInfo, TArray<FString>(), TArray<FString>(), TEXT(""), UnusedSyncLocation, ModuleInfo, OutFailReason) )
		{
			CreatedFiles.Add(NewCppFilename);
		}
		else
		{
			DeleteCreatedFiles(NewCppPath, CreatedFiles);
			return EAddCodeToProjectResult::FailedToAddCode;
		}
	}

	SlowTask.EnterProgressFrame();

	TArray<FString> CreatedFilesForExternalAppRead;
	CreatedFilesForExternalAppRead.Reserve(CreatedFiles.Num());
	for (const FString& CreatedFile : CreatedFiles)
	{
		CreatedFilesForExternalAppRead.Add( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CreatedFile) );
	}

	bool bGenerateProjectFiles = true;

	// First see if we can avoid a full generation by adding the new files to an already open project
	if ( bProjectHadCodeFiles && FSourceCodeNavigation::AddSourceFiles(CreatedFilesForExternalAppRead) )
	{
		// We successfully added the new files to the solution, but we still need to run UBT with -gather to update any UBT makefiles
		if ( FDesktopPlatformModule::Get()->InvalidateMakefiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn) )
		{
			// We managed the gather, so we can skip running the full generate
			bGenerateProjectFiles = false;
		}
	}
	
	if ( bGenerateProjectFiles )
	{
		// Generate project files if we happen to be using a project file.
		if ( !FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn) )
		{
			OutFailReason = LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files.");
			return EAddCodeToProjectResult::FailedToHotReload;
		}
	}

	SlowTask.EnterProgressFrame();

	// Mark the files for add in SCC
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable() )
	{
		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), CreatedFilesForExternalAppRead);
	}

	SlowTask.EnterProgressFrame( 1.0f, LOCTEXT("CompilingCPlusPlusCode", "Compiling new C++ code.  Please wait..."));

	OutHeaderFilePath = NewHeaderFilename;
	OutCppFilePath = NewCppFilename;

	if (!bProjectHadCodeFiles)
	{
		// This is the first time we add code to this project so compile its game DLL
		const FString GameModuleName = FApp::GetProjectName();
		check(ModuleInfo.ModuleName == GameModuleName);

		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
		const bool bReloadAfterCompiling = true;
		const bool bForceCodeProject = true;
		const bool bFailIfGeneratedCodeChanges = false;
		if (!HotReloadSupport.RecompileModule(*GameModuleName, bReloadAfterCompiling, *GWarn, bFailIfGeneratedCodeChanges, bForceCodeProject))
		{
			OutFailReason = LOCTEXT("FailedToCompileNewGameModule", "Failed to compile newly created game module.");
			return EAddCodeToProjectResult::FailedToHotReload;
		}

		// Notify that we've created a brand new module
		FSourceCodeNavigation::AccessOnNewModuleAdded().Broadcast(*GameModuleName);
	}
	else if (GetDefault<UEditorPerProjectUserSettings>()->bAutomaticallyHotReloadNewClasses)
	{
		FModuleStatus ModuleStatus;
		const FName ModuleFName = *ModuleInfo.ModuleName;
		if (ensure(FModuleManager::Get().QueryModule(ModuleFName, ModuleStatus)))
		{
			// Compile the module that the class was added to so that the newly added class with appear in the Content Browser
			TArray<UPackage*> PackagesToRebind;
			if (ModuleStatus.bIsLoaded)
			{
				const bool bIsHotReloadable = FModuleManager::Get().DoesLoadedModuleHaveUObjects(ModuleFName);
				if (bIsHotReloadable)
				{
					// Is there a UPackage with the same name as this module?
					const FString PotentialPackageName = FString(TEXT("/Script/")) + ModuleInfo.ModuleName;
					UPackage* Package = FindPackage(nullptr, *PotentialPackageName);
					if (Package)
					{
						PackagesToRebind.Add(Package);
					}
				}
			}

			IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
			if (PackagesToRebind.Num() > 0)
			{
				// Perform a hot reload
				const bool bWaitForCompletion = true;			
				ECompilationResult::Type CompilationResult = HotReloadSupport.RebindPackages( PackagesToRebind, TArray<FName>(), bWaitForCompletion, *GWarn );
				if( CompilationResult != ECompilationResult::Succeeded && CompilationResult != ECompilationResult::UpToDate )
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToHotReloadModuleFmt", "Failed to automatically hot reload the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return EAddCodeToProjectResult::FailedToHotReload;
				}
			}
			else
			{
				// Perform a regular unload, then reload
				const bool bReloadAfterRecompile = true;
				const bool bForceCodeProject = false;
				const bool bFailIfGeneratedCodeChanges = true;
				if (!HotReloadSupport.RecompileModule(ModuleFName, bReloadAfterRecompile, *GWarn, bFailIfGeneratedCodeChanges, bForceCodeProject))
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToCompileModuleFmt", "Failed to automatically compile the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return EAddCodeToProjectResult::FailedToHotReload;
				}
			}
		}
	}

	return EAddCodeToProjectResult::Succeeded;
}

bool GameProjectUtils::FindSourceFileInProject(const FString& InFilename, const FString& InSearchPath, FString& OutPath)
{
	TArray<FString> Filenames;
	IFileManager::Get().FindFilesRecursive(Filenames, *InSearchPath, *InFilename, true, false, false);
	
	if(Filenames.Num())
	{
		// Assume it's the first match (we should really only find a single file with a given name within a project anyway)
		OutPath = Filenames[0];
		return true;
	}

	return false;
}


void GameProjectUtils::HarvestCursorSyncLocation( FString& FinalOutput, FString& OutSyncLocation )
{
	OutSyncLocation.Empty();

	// Determine the cursor focus location if this file will by synced after creation
	TArray<FString> Lines;
	FinalOutput.ParseIntoArray( Lines, TEXT( "\n" ), false );
	for( int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx )
	{
		const FString& Line = Lines[ LineIdx ];
		int32 CharLoc = Line.Find( TEXT( "%CURSORFOCUSLOCATION%" ) );
		if( CharLoc != INDEX_NONE )
		{
			// Found the sync marker
			OutSyncLocation = FString::Printf( TEXT( "%d:%d" ), LineIdx + 1, CharLoc + 1 );
			break;
		}
	}

	// If we did not find the sync location, just sync to the top of the file
	if( OutSyncLocation.IsEmpty() )
	{
		OutSyncLocation = TEXT( "1:1" );
	}

	// Now remove the cursor focus marker
	FinalOutput = FinalOutput.Replace(TEXT("%CURSORFOCUSLOCATION%"), TEXT(""), ESearchCase::CaseSensitive);
}

bool GameProjectUtils::InsertFeaturePacksIntoINIFile(const FProjectInformation& InProjectInfo, FText& OutFailReason)
{
	const FString ProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
	const FString TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	const FString SrcFolder = FPaths::GetPath(InProjectInfo.TemplateFile);
	const FString DestFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);

	const FString ProjectConfigPath = DestFolder / TEXT("Config");
	const FString IniFilename = ProjectConfigPath / TEXT("DefaultGame.ini");

	TArray<FString> PackList;

	// First the starter content
	if (InProjectInfo.bCopyStarterContent)
	{
		FString StarterPack;
		if (InProjectInfo.TargetedHardware == EHardwareClass::Mobile)
		{
			StarterPack = TEXT("InsertPack=(PackSource=\"MobileStarterContent") + DefaultFeaturePackExtension + TEXT(",PackName=\"StarterContent\")");
		}
		else
		{
			StarterPack = TEXT("InsertPack=(PackSource=\"StarterContent")  + DefaultFeaturePackExtension + TEXT(",PackName=\"StarterContent\")");
		}
		PackList.Add(StarterPack);
	}
	
	if (PackList.Num() != 0)
	{
		FString FileOutput;
		if(FPaths::FileExists(IniFilename) && !FFileHelper::LoadFileToString(FileOutput, *IniFilename))
		{
			OutFailReason = LOCTEXT("FailedToReadIni", "Could not read INI file to insert feature packs");
			return false;
		}

		FileOutput += LINE_TERMINATOR;
		FileOutput += TEXT("[StartupActions]");
		FileOutput += LINE_TERMINATOR;
		FileOutput += TEXT("bAddPacks=True");
		FileOutput += LINE_TERMINATOR;
		for (int32 iLine = 0; iLine < PackList.Num(); ++iLine)
		{
			FileOutput += PackList[iLine] + LINE_TERMINATOR;
		}

		if (!FFileHelper::SaveStringToFile(FileOutput, *IniFilename))
		{
			OutFailReason = LOCTEXT("FailedToWriteIni", "Could not write INI file to insert feature packs");
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::AddSharedContentToProject(const FProjectInformation &InProjectInfo, TArray<FString> &CreatedFiles, FText& OutFailReason)
{
	//const FString TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	const FString SrcFolder = FPaths::GetPath(InProjectInfo.TemplateFile);
	const FString DestFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);

	const FString ProjectConfigPath = DestFolder / TEXT("Config");
	const FString IniFilename = ProjectConfigPath / TEXT("DefaultGame.ini");
	
	// Now any packs specified in the template def.
	UTemplateProjectDefs* TemplateDefs = LoadTemplateDefs(SrcFolder);
	if (TemplateDefs != NULL)
	{
		EFeaturePackDetailLevel RequiredDetail = EFeaturePackDetailLevel::High;
		if (InProjectInfo.TargetedHardware == EHardwareClass::Mobile)
		{
			RequiredDetail = EFeaturePackDetailLevel::Standard;
		}


		TUniquePtr<FFeaturePackContentSource> TempFeaturePack = MakeUnique<FFeaturePackContentSource>();
		bool bCopied = TempFeaturePack->InsertAdditionalResources(TemplateDefs->SharedContentPacks,RequiredDetail, DestFolder,CreatedFiles);
		if( bCopied == false )
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("TemplateName"), FText::FromString(SrcFolder));
			OutFailReason = FText::Format(LOCTEXT("SharedResourceError", "Error adding shared resources for '{TemplateName}'."), Args);
			return false;		
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
