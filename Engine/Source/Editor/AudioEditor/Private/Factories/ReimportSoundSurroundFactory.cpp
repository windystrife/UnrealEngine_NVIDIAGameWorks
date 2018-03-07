// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/ReimportSoundSurroundFactory.h"
#include "Logging/MessageLog.h"
#include "Sound/SoundWave.h"
#include "Misc/Paths.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "AudioEditorFactories"

UReimportSoundSurroundFactory::UReimportSoundSurroundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	SupportedClass = USoundWave::StaticClass();
	Formats.Add(TEXT("wav;Sound"));

	bCreateNew = false;
	CueVolume = 0.75f;
}

bool UReimportSoundSurroundFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if (SoundWave && SoundWave->NumChannels > 2)
	{
		FString SourceFilename = SoundWave->AssetImportData->GetFirstFilename();
		if (!SourceFilename.IsEmpty() && FactoryCanImport(SourceFilename))
		{
			// Get filename with speaker location removed
			FString BaseFilename = FPaths::GetBaseFilename(SourceFilename).LeftChop(3);
			FString FileExtension = FPaths::GetExtension(SourceFilename, true);
			FString FilePath = FPaths::GetPath(SourceFilename);

			// Add a filename for each speaker location we have Channel Size data for
			for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->ChannelSizes.Num(); ++ChannelIndex)
			{
				if (SoundWave->ChannelSizes[ChannelIndex])
				{
					OutFilenames.Add(FilePath + TEXT("//") + BaseFilename + SurroundSpeakerLocations[ChannelIndex] + FileExtension);
				}
			}
		}
		else
		{
			// We failed to generate possible filenames, fill the array with a blank string for each channel
			for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
			{
				OutFilenames.Add(FString());
			}
		}

		// Store these for later use
		ReimportPaths = OutFilenames;

		return true;
	}
	return false;
}

void UReimportSoundSurroundFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USoundWave* SoundWave = Cast<USoundWave>(Obj);
	if (SoundWave)
	{
		ReimportPaths = NewReimportPaths;
	}
}

EReimportResult::Type UReimportSoundSurroundFactory::Reimport(UObject* Obj)
{
	// Only handle valid sound node waves
	if (!Obj || !Obj->IsA(USoundWave::StaticClass()))
	{
		return EReimportResult::Failed;
	}

	USoundWave* SoundWave = Cast<USoundWave>(Obj);

	// Holds the warnings for the message log.
	FMessageLog EditorErrors("EditorErrors");
	FText NameText = FText::FromString(SoundWave->GetName());

	bool bSourceReimported = false;

	for (int32 PathIndex = 0; PathIndex < ReimportPaths.Num(); ++PathIndex)
	{
		FString Filename(ReimportPaths[PathIndex]);

		// If there is no file path provided, can't reimport from source
		if (!Filename.Len())
		{
			// Since this is a new system most sound node waves don't have paths, so logging has been commented out
			//UE_LOG(LogEditorFactories, Warning, TEXT("-- cannot reimport: sound node wave resource does not have path stored."));
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningNoFilename", "{NameText}: Attempt to reimport empty file name."), Arguments));
			continue;
		}

		FText FilenameText = FText::FromString(Filename);

		const FString FileExtension = FPaths::GetExtension(Filename);
		const bool bIsWav = (FCString::Stricmp(*FileExtension, TEXT("WAV")) == 0);

		// Only handle WAV files
		if (!bIsWav)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningFormat", "{NameText}: Incorrect File Format - {FilenameText}"), Arguments));
			continue;
		}

		// Ensure that the file provided by the path exists
		if (IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningNoFile", "{NameText}: Source file cannot be found - {FilenameText}"), Arguments));
			continue;
		}

		FString SpeakerLocation = FPaths::GetBaseFilename(Filename).Right(3);
		FName ImportName = *(SoundWave->GetName() + SpeakerLocation);
		bool OutCanceled = false;

		if (ImportObject(SoundWave->GetClass(), SoundWave->GetOuter(), ImportName, RF_Public | RF_Standalone, Filename, nullptr, OutCanceled) != nullptr)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Info(FText::Format(LOCTEXT("SurroundWarningImportSucceeded", "{NameText}: Import successful - {FilenameText}"), Arguments));

			// Mark the package dirty after the successful import
			SoundWave->MarkPackageDirty();
			SoundWave->bNeedsThumbnailGeneration = true;

			bSourceReimported = true;
		}
		else if (!OutCanceled)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("NameText"), NameText);
			Arguments.Add(TEXT("FilenameText"), FilenameText);
			EditorErrors.Warning(FText::Format(LOCTEXT("SurroundWarningImportFailed", "{NameText}: Import failed - {FilenameText}"), Arguments));
		}
	}

	EditorErrors.Notify(LOCTEXT("SurroundWarningDescription", "Some files could not be reimported."), EMessageSeverity::Warning);

	return bSourceReimported ? EReimportResult::Succeeded : EReimportResult::Failed;
}

int32 UReimportSoundSurroundFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE
