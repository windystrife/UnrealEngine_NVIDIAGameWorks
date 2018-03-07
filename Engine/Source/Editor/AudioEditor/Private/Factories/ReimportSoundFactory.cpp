// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/ReimportSoundFactory.h"
#include "Sound/SoundWave.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"
#include "AudioEditorModule.h"
#include "HAL/FileManager.h"

UReimportSoundFactory::UReimportSoundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));

	bCreateNew = false;
	bAutoCreateCue = false;
	bIncludeAttenuationNode = false;
	bIncludeModulatorNode = false;
	bIncludeLoopingNode = false;
	CueVolume = 0.75f;
}

bool UReimportSoundFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if (SoundWave && SoundWave->NumChannels < 3)
	{
		SoundWave->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UReimportSoundFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if (SoundWave && ensure(NewReimportPaths.Num() == 1))
	{
		SoundWave->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UReimportSoundFactory::Reimport(UObject* Obj)
{
	// Only handle valid sound node waves
	if (!Obj || !Obj->IsA(USoundWave::StaticClass()))
	{
		return EReimportResult::Failed;
	}

	USoundWave* SoundWave = Cast<USoundWave>(Obj);

	const FString Filename = SoundWave->AssetImportData->GetFirstFilename();
	const FString FileExtension = FPaths::GetExtension(Filename);
	const bool bIsWav = (FCString::Stricmp(*FileExtension, TEXT("WAV")) == 0);

	// Only handle WAV files
	if (!bIsWav)
	{
		return EReimportResult::Failed;
	}
	// If there is no file path provided, can't reimport from source
	if (!Filename.Len())
	{
		// Since this is a new system most sound node waves don't have paths, so logging has been commented out
		//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: sound node wave resource does not have path stored."));
		return EReimportResult::Failed;
	}

	UE_LOG(LogAudioEditor, Log, TEXT("Performing atomic reimport of [%s]"), *Filename);

	// Ensure that the file provided by the path exists
	if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogAudioEditor, Warning, TEXT("-- cannot reimport: source file cannot be found."));
		return EReimportResult::Failed;
	}

	// Suppress the import overwrite dialog, we want to keep existing settings when re-importing
	USoundFactory::SuppressImportOverwriteDialog();

	bool OutCanceled = false;
	if (ImportObject(SoundWave->GetClass(), SoundWave->GetOuter(), *SoundWave->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr)
	{
		UE_LOG(LogAudioEditor, Log, TEXT("-- imported successfully"));

		SoundWave->AssetImportData->Update(Filename);
		SoundWave->MarkPackageDirty();
		SoundWave->bNeedsThumbnailGeneration = true;
	}
	else if (OutCanceled)
	{
		UE_LOG(LogAudioEditor, Warning, TEXT("-- import canceled"));
	}
	else
	{
		UE_LOG(LogAudioEditor, Warning, TEXT("-- import failed"));
	}

	return EReimportResult::Succeeded;
}

int32 UReimportSoundFactory::GetPriority() const
{
	return ImportPriority;
}
