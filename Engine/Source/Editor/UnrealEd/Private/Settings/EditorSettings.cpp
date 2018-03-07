// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Settings/EditorSettings.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/UnrealType.h"
#include "Interfaces/IProjectManager.h"

UEditorSettings::UEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCopyStarterContentPreference = false;
	bEditorAnalyticsEnabled_DEPRECATED = true;
	AutoScalabilityWorkScaleAmount = 1;
}

void UEditorSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	const FName Name = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;
	if (Name == FName(TEXT("bLoadTheMostRecentlyLoadedProjectAtStartup")))
	{
		const FString& AutoLoadProjectFileName = IProjectManager::Get().GetAutoLoadProjectFileName();
		if ( bLoadTheMostRecentlyLoadedProjectAtStartup )
		{
			// Form or overwrite the file that is read at load to determine the most recently loaded project file
			FFileHelper::SaveStringToFile(FPaths::GetProjectFilePath(), *AutoLoadProjectFileName);
		}
		else
		{
			// Remove the file. It's possible for bLoadTheMostRecentlyLoadedProjectAtStartup to be set before FPaths::GetProjectFilePath() is valid, so we need to distinguish the two cases.
			IFileManager::Get().Delete(*AutoLoadProjectFileName);
		}
	}


	SaveConfig(CPF_Config);
}

void UEditorSettings::LoadScalabilityBenchmark()
{
	check(!GEditorSettingsIni.IsEmpty());

	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	Scalability::FQualityLevels Temporary;

	if (IsScalabilityBenchmarkValid())
	{
		GConfig->GetFloat(Section, TEXT("ResolutionQuality"), Temporary.ResolutionQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("ViewDistanceQuality"), Temporary.ViewDistanceQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("AntiAliasingQuality"), Temporary.AntiAliasingQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("ShadowQuality"), Temporary.ShadowQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("PostProcessQuality"), Temporary.PostProcessQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("TextureQuality"), Temporary.TextureQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("EffectsQuality"), Temporary.EffectsQuality, GEditorSettingsIni);
		GConfig->GetInt(Section, TEXT("FoliageQuality"), Temporary.FoliageQuality, GEditorSettingsIni);
		EngineBenchmarkResult = Temporary;
	}
}

void UEditorSettings::AutoApplyScalabilityBenchmark()
{
	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	FScopedSlowTask SlowTask(0, NSLOCTEXT("UnrealEd", "RunningEngineBenchmark", "Running engine benchmark..."));
	SlowTask.MakeDialog();


	Scalability::FQualityLevels Temporary = Scalability::BenchmarkQualityLevels(AutoScalabilityWorkScaleAmount);

	GConfig->SetBool(Section, TEXT("Valid"), true, GEditorSettingsIni);
	GConfig->SetFloat(Section, TEXT("ResolutionQuality"), Temporary.ResolutionQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("ViewDistanceQuality"), Temporary.ViewDistanceQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("AntiAliasingQuality"), Temporary.AntiAliasingQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("ShadowQuality"), Temporary.ShadowQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("PostProcessQuality"), Temporary.PostProcessQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("TextureQuality"), Temporary.TextureQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("EffectsQuality"), Temporary.EffectsQuality, GEditorSettingsIni);
	GConfig->SetInt(Section, TEXT("FoliageQuality"), Temporary.FoliageQuality, GEditorSettingsIni);

	Scalability::SetQualityLevels(Temporary);
	Scalability::SaveState(GEditorSettingsIni);
}

bool UEditorSettings::IsScalabilityBenchmarkValid() const
{
	const TCHAR* Section = TEXT("EngineBenchmarkResult");

	bool bIsValid = false;
	GConfig->GetBool(Section, TEXT("Valid"), bIsValid, GEditorSettingsIni);

	return bIsValid;
}
