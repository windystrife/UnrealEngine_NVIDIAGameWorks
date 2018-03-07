// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/SoundFactory.h"
#include "AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Audio.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeAttenuation.h"
#include "AudioDeviceManager.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "EditorFramework/AssetImportData.h"

static bool bSoundFactorySuppressImportOverwriteDialog = false;

static void InsertSoundNode(USoundCue* SoundCue, UClass* NodeClass, int32 NodeIndex)
{
	USoundNode* SoundNode = SoundCue->ConstructSoundNode<USoundNode>(NodeClass);

	// If this node allows >0 children but by default has zero - create a connector for starters
	if (SoundNode->GetMaxChildNodes() > 0 && SoundNode->ChildNodes.Num() == 0)
	{
		SoundNode->CreateStartingConnectors();
	}

	SoundNode->GraphNode->NodePosX = -150 * NodeIndex - 100;
	SoundNode->GraphNode->NodePosY = -35;

	// Link the node to the cue.
	SoundNode->ChildNodes[0] = SoundCue->FirstNode;

	// Link the attenuation node to root.
	SoundCue->FirstNode = SoundNode;

	SoundCue->LinkGraphNodesFromSoundNodes();
}

static void CreateSoundCue(USoundWave* Sound, UObject* InParent, EObjectFlags Flags, bool bIncludeAttenuationNode, bool bIncludeModulatorNode, bool bIncludeLoopingNode, float CueVolume)
{
	// then first create the actual sound cue
	FString SoundCueName = FString::Printf(TEXT("%s_Cue"), *Sound->GetName());

	// Create sound cue and wave player
	USoundCue* SoundCue = NewObject<USoundCue>(InParent, *SoundCueName, Flags);
	USoundNodeWavePlayer* WavePlayer = SoundCue->ConstructSoundNode<USoundNodeWavePlayer>();

	int32 NodeIndex = (int32)bIncludeAttenuationNode + (int32)bIncludeModulatorNode + (int32)bIncludeLoopingNode;

	WavePlayer->GraphNode->NodePosX = -150 * NodeIndex - 100;
	WavePlayer->GraphNode->NodePosY = -35;

	// Apply the initial volume.
	SoundCue->VolumeMultiplier = CueVolume;

	WavePlayer->SetSoundWave(Sound);
	SoundCue->FirstNode = WavePlayer;
	SoundCue->LinkGraphNodesFromSoundNodes();

	if (bIncludeLoopingNode)
	{
		WavePlayer->bLooping = true;
	}

	if (bIncludeModulatorNode)
	{
		InsertSoundNode(SoundCue, USoundNodeModulator::StaticClass(), --NodeIndex);
	}

	if (bIncludeAttenuationNode)
	{
		InsertSoundNode(SoundCue, USoundNodeAttenuation::StaticClass(), --NodeIndex);
	}

	// Make sure the content browser finds out about this newly-created object.  This is necessary when sound
	// cues are created automatically after creating a sound node wave.  See use of bAutoCreateCue in USoundTTSFactory.
	if ((Flags & (RF_Public | RF_Standalone)) != 0)
	{
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(SoundCue);
	}
}


USoundFactory::USoundFactory(const FObjectInitializer& ObjectInitializer)
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
	CuePackageSuffix = TEXT("_Cue");
	bEditorImport = true;
}

UObject* USoundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	EObjectFlags		Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const uint8*&		Buffer,
	const uint8*			BufferEnd,
	FFeedbackContext*	Warn
	)
{
	FEditorDelegates::OnAssetPreImport.Broadcast(this, Class, InParent, Name, FileType);

	if (FCString::Stricmp(FileType, TEXT("WAV")) == 0)
	{
		// create the group name for the cue
		const FString GroupName = InParent->GetFullGroupName(false);
		FString CuePackageName = InParent->GetOutermost()->GetName();
		CuePackageName += CuePackageSuffix;
		if (GroupName.Len() > 0 && GroupName != TEXT("None"))
		{
			CuePackageName += TEXT(".");
			CuePackageName += GroupName;
		}

		// validate the cue's group
		FText Reason;
		const bool bCuePathIsValid = FName(*CuePackageSuffix).IsValidGroupName(Reason);
		const bool bMoveCue = CuePackageSuffix.Len() > 0 && bCuePathIsValid && bAutoCreateCue;
		if (bAutoCreateCue)
		{
			if (!bCuePathIsValid)
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_ImportFailed_f", "Import failed for {0}: {1}"), FText::FromString(CuePackageName), Reason));
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				return nullptr;
			}
		}

		// if we are creating the cue move it when necessary
		UPackage* CuePackage = bMoveCue ? CreatePackage(nullptr, *CuePackageName) : nullptr;

		// if the sound already exists, remember the user settings
		USoundWave* ExistingSound = FindObject<USoundWave>(InParent, *Name.ToString());

		TArray<UAudioComponent*> ComponentsToRestart;
		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		if (AudioDeviceManager && ExistingSound)
		{
			// Will block internally on audio thread completing outstanding commands
			AudioDeviceManager->StopSoundsUsingResource(ExistingSound, &ComponentsToRestart);
		}

		bool bUseExistingSettings = bSoundFactorySuppressImportOverwriteDialog;

		if (ExistingSound && !bSoundFactorySuppressImportOverwriteDialog && !GIsAutomationTesting)
		{
			DisplayOverwriteOptionsDialog(FText::Format(
				NSLOCTEXT("SoundFactory", "ImportOverwriteWarning", "You are about to import '{0}' over an existing sound."),
				FText::FromName(Name)));

			switch (OverwriteYesOrNoToAllState)
			{

			case EAppReturnType::Yes:
			case EAppReturnType::YesAll:
			{
				// Overwrite existing settings
				bUseExistingSettings = false;
				break;
			}
			case EAppReturnType::No:
			case EAppReturnType::NoAll:
			{
				// Preserve existing settings
				bUseExistingSettings = true;
				break;
			}
			default:
			{
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				return nullptr;
			}
			}
		}

		// Reset the flag back to false so subsequent imports are not suppressed unless the code explicitly suppresses it
		bSoundFactorySuppressImportOverwriteDialog = false;

		TArray<uint8> RawWaveData;
		RawWaveData.Empty(BufferEnd - Buffer);
		RawWaveData.AddUninitialized(BufferEnd - Buffer);
		FMemory::Memcpy(RawWaveData.GetData(), Buffer, RawWaveData.Num());

		// Read the wave info and make sure we have valid wave data
		FWaveModInfo WaveInfo;
		FString ErrorMessage;
		if (WaveInfo.ReadWaveInfo(RawWaveData.GetData(), RawWaveData.Num(), &ErrorMessage))
		{
			if (*WaveInfo.pBitsPerSample != 16)
			{
				WaveInfo.ReportImportFailure();
				Warn->Logf(ELogVerbosity::Error, TEXT("Currently, only 16 bit WAV files are supported (%s)."), *Name.ToString());
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				return nullptr;
			}

			if (*WaveInfo.pChannels != 1 && *WaveInfo.pChannels != 2)
			{
				WaveInfo.ReportImportFailure();
				Warn->Logf(ELogVerbosity::Error, TEXT("Currently, only mono or stereo WAV files are supported (%s)."), *Name.ToString());
				FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
				return nullptr;
			}
		}
		else
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Unable to read wave file '%s' - \"%s\""), *Name.ToString(), *ErrorMessage);
			FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
			return nullptr;
		}

		// Use pre-existing sound if it exists and we want to keep settings,
		// otherwise create new sound and import raw data.
		USoundWave* Sound = (bUseExistingSettings && ExistingSound) ? ExistingSound : NewObject<USoundWave>(InParent, Name, Flags);

		if (bUseExistingSettings && ExistingSound)
		{
			// Clear resources so that if it's already been played, it will reload the wave data
			Sound->FreeResources();
		}

		// Store the current file path and timestamp for re-import purposes
		Sound->AssetImportData->Update(CurrentFilename);

		// Compressed data is now out of date.
		Sound->InvalidateCompressedData();

		Sound->RawData.Lock(LOCK_READ_WRITE);
		void* LockedData = Sound->RawData.Realloc(BufferEnd - Buffer);
		FMemory::Memcpy(LockedData, Buffer, BufferEnd - Buffer);
		Sound->RawData.Unlock();

		// Calculate duration.
		int32 DurationDiv = *WaveInfo.pChannels * *WaveInfo.pBitsPerSample * *WaveInfo.pSamplesPerSec;
		if (DurationDiv)
		{
			Sound->Duration = *WaveInfo.pWaveDataSize * 8.0f / DurationDiv;
		}
		else
		{
			Sound->Duration = 0.0f;
		}

		Sound->SampleRate = *WaveInfo.pSamplesPerSec;
		Sound->NumChannels = *WaveInfo.pChannels;

		FEditorDelegates::OnAssetPostImport.Broadcast(this, Sound);

		if (ExistingSound && bUseExistingSettings)
		{
			// Call PostEditChange() to update text to speech
			Sound->PostEditChange();
		}

		// if we're auto creating a default cue
		if (bAutoCreateCue)
		{
			CreateSoundCue(Sound, bMoveCue ? CuePackage : InParent, Flags, bIncludeAttenuationNode, bIncludeModulatorNode, bIncludeLoopingNode, CueVolume);
		}

		for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToRestart.Num(); ++ComponentIndex)
		{
			ComponentsToRestart[ComponentIndex]->Play();
		}

		Sound->bNeedsThumbnailGeneration = true;

		return Sound;
	}
	else
	{
		// Unrecognized.
		Warn->Logf(ELogVerbosity::Error, TEXT("Unrecognized sound format '%s' in %s"), FileType, *Name.ToString());
		FEditorDelegates::OnAssetPostImport.Broadcast(this, nullptr);
		return nullptr;
	}
}

void USoundFactory::SuppressImportOverwriteDialog()
{
	bSoundFactorySuppressImportOverwriteDialog = true;
}
