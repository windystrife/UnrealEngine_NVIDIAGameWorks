// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Containers/List.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS

namespace UAudio
{
	static IUnrealAudioModule* UnrealAudioModuleSystem = nullptr;
	static bool bTestInitialized = false;

	void FUnrealAudioModule::InitializeSystemTests(IUnrealAudioModule* Module)
	{
		UnrealAudioModuleSystem = Module;
		bTestInitialized = true;
	}

	// this is called from the system thread
	void FUnrealAudioModule::UpdateSystemTests()
	{

	}

	class FTestTimer
	{
	public:
		FTestTimer()
			: TotalTime(0.0)
			, StartTime(0.0)
		{
		}

		FTestTimer(double InTotalTimeSeconds)
			: TotalTime(InTotalTimeSeconds)
			, StartTime(FPlatformTime::Seconds())
		{
		}

		void Start(double Time)
		{
			StartTime = FPlatformTime::Seconds();
			TotalTime = Time;
		}

		double GetTimeFraction()
		{
			return FMath::Min((FPlatformTime::Seconds() - StartTime) / TotalTime, 1.0);
		}

		bool IsDone()
		{
			return GetTimeFraction() >= 1.0;
		}

	private:
		double TotalTime;
		double StartTime;
	};


	static FVector GetRandomPosition()
	{
		return FVector(FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f), FMath::FRandRange(-100.0f, 100.0f));
	}


	bool TestEmitterManager()
	{
		if (!bTestInitialized)
		{
			return false;
		}
		
		// Lets create a bunch of initial emitters
		TLinkedList<TSharedPtr<IEmitter>>* EmittersHead = nullptr;
		int32 NumEmitters = 100;
		for (int32 i = 0; i < NumEmitters; ++i)
		{
			TSharedPtr<IEmitter> Emitter = UnrealAudioModuleSystem->EmitterCreate();
			UE_LOG(LogUnrealAudio, Display, TEXT("[%d] Create"), Emitter->GetId());

			Emitter->SetPosition(GetRandomPosition());
			TLinkedList<TSharedPtr<IEmitter>>* EmitterLink = new TLinkedList<TSharedPtr<IEmitter>>(Emitter);
			if (EmittersHead == nullptr)
			{
				EmittersHead = EmitterLink;
			}
			else
			{
				EmitterLink->LinkHead(EmittersHead);
			}
		}

		// For 30 seconds, lets randomly set emitter positions from this thread
		FTestTimer TestTimer(30);
		while (!TestTimer.IsDone())
		{
			TLinkedList<TSharedPtr<IEmitter>>* Link = EmittersHead;
			while (Link)
			{
				// 50-50 chance to change position
				if (FMath::FRand() < 0.5f)
				{
					FVector Pos = GetRandomPosition();
					UE_LOG(LogUnrealAudio, Display, TEXT("[%d] SetPosition: (%.2f, %.2f, %.2f)"), (*Link)->Get()->GetId(), Pos.X, Pos.Y, Pos.Z);
					(*Link)->Get()->SetPosition(Pos);
				}
				// 5% chance to be released
				else if (FMath::FRand() < 0.05f)
				{
					UE_LOG(LogUnrealAudio, Display, TEXT("[%d] Release"), (*Link)->Get()->GetId());
					(*Link)->Get()->Release();

					// Unlink the emitter from our linked list and delete it
					TLinkedList<TSharedPtr<IEmitter>>* NextLink = Link->GetNextLink();
					Link->Unlink();
					delete Link;
					Link = NextLink;
				}
				else
				{
					Link = Link->GetNextLink();
				}
			}

			// Randomly add a bunch of emitters
			for (int32 i = 0; i < 10; ++i)
			{
				if (FMath::FRand() < 0.1f)
				{
					TSharedPtr<IEmitter> Emitter = UnrealAudioModuleSystem->EmitterCreate();
					UE_LOG(LogUnrealAudio, Display, TEXT("[%d] Create"), Emitter->GetId());
					Emitter->SetPosition(GetRandomPosition());
					TLinkedList<TSharedPtr<IEmitter>>* EmitterLink = new TLinkedList<TSharedPtr<IEmitter>>(Emitter);
					EmitterLink->LinkHead(EmittersHead);
				} //-V773
			}

			FPlatformProcess::Sleep(0.033f);
		}

		return true;
	}

	bool TestVoiceManager(const FString& FolderOrPath)
	{
		if (!bTestInitialized)
		{
			return false;
		}

		TSharedPtr<ISoundFile> SoundFile = UnrealAudioModuleSystem->StreamSoundFile(*FolderOrPath);

		FVoiceInitializationParams InitParams;
		InitParams.SoundFile = SoundFile;
		InitParams.bIsLooping = true;
		InitParams.BaselinePitchScale = 1.0f;
		InitParams.BaselineVolumeScale = 1.0f;
		InitParams.PriorityWeight = 1.0f;

		TSharedPtr<IVoice> Voice = UnrealAudioModuleSystem->VoiceCreate(InitParams);
		EVoiceError::Type Error = Voice->Play();
		check(Error == EVoiceError::NONE);
		
		FTestTimer VolumeTimer;
		FTestTimer PitchTimer;
		float VolumeProduct = 0.0f;
		float PitchProduct = 0.0f;
		float VolumeTime = 0.0f;
		float VolumeTarget = 0.0f;
		float PitchTime = 0.0f;
		float PitchTarget = 0.0f;

		while (true)
		{
			// Update the module on the main thread
			UnrealAudioModuleSystem->Update();

			if (VolumeTimer.IsDone())
			{
				VolumeTime = FMath::FRandRange(1.0f, 10.0f);
				VolumeTarget = FMath::FRandRange(0.0f, 1.0f);

				Error = Voice->SetVolumeScale(VolumeTarget, VolumeTime);
				check(Error == EVoiceError::NONE);

				VolumeTimer.Start(VolumeTime);
			}

			if (PitchTimer.IsDone())
			{
				PitchTime = FMath::FRandRange(1.0f, 10.0f);
				PitchTarget = FMath::FRandRange(0.01f, 4.0f);

				Error = Voice->SetPitchScale(PitchTarget, PitchTime);
				check(Error == EVoiceError::NONE);

				PitchTimer.Start(PitchTime);
			}

 			Voice->GetVolumeProduct(VolumeProduct);
 			Voice->GetPitchProduct(PitchProduct);

			float CurrentVolumeTime = VolumeTime * VolumeTimer.GetTimeFraction();
			float CurrentPitchTime = PitchTime * PitchTimer.GetTimeFraction();

			UE_LOG(LogUnrealAudio, Display, TEXT("V: %.2f [%.2f, %.2f/%.2f] P: %.2f [%.2f, %.2f/%.2f]"), 
				VolumeProduct, 
				VolumeTarget,
				CurrentVolumeTime,
				VolumeTime,
				
				PitchProduct,
				PitchTarget,
				CurrentPitchTime,
				PitchTime
			);

			FPlatformProcess::Sleep(0.033f);
		}

		return true;
	}

	static bool TestSingleFileLoad(const FString& Path)
	{
		{
			// Test loading a single file
			UE_LOG(LogUnrealAudio, Display, TEXT("Loading sound file %s"), *Path);
			TSharedPtr<ISoundFile> SoundFile = UnrealAudioModuleSystem->LoadSoundFile(*Path);

			// Block the thread till the sound file is loaded
			ESoundFileState::Type State;
			SoundFile->GetState(State);
			while (State != ESoundFileState::LOADED)
			{
				FPlatformProcess::Sleep(0.033f);
				UE_LOG(LogUnrealAudio, Display, TEXT("Loading..."));
				SoundFile->GetState(State);
			}

			UE_LOG(LogUnrealAudio, Display, TEXT("Loaded."));

			// Try loading it again
			UE_LOG(LogUnrealAudio, Display, TEXT("Attempting to reload sound file from cache to new ISoundFile ptr."));
			TSharedPtr<ISoundFile> NewSoundFile = UnrealAudioModuleSystem->LoadSoundFile(*Path);

			// Since it's already loaded, this should immediately be loaded
			NewSoundFile->GetState(State);
			check(State == ESoundFileState::LOADED);

			UE_LOG(LogUnrealAudio, Display, TEXT("Sound file already loaded: Yes"));

			// Let the sound file shared ptrs fall out of scope, this will cause the sound files
			// to decref the sound file manager internally and trigger the time-based flush
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Waiting until the sound file is flushed from the sound file cache...."));

		// Wait now until the loaded file is flushed from the cache
		float TotalTime = 0.0f;
		int32 NumSoundFilesLoaded = 1;
		while (NumSoundFilesLoaded != 0)
		{
			UnrealAudioModuleSystem->Update();
			NumSoundFilesLoaded = UnrealAudioModuleSystem->GetNumSoundFilesLoaded();

			UE_LOG(LogUnrealAudio, Display, TEXT("Num Loaded: %d (%.2f seconds)"), NumSoundFilesLoaded, TotalTime);

			FPlatformProcess::Sleep(0.033f);
			TotalTime += 0.033f;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Sound file memory cache has flushed in %.2f seconds"), TotalTime);
		return true;
	}

	static bool TestArrayOfFilesLoading(TArray<FString>& Files)
	{
		TArray<TSharedPtr<ISoundFile>> LoadedFiles;

		float CurrentTime = 0.0;
		float TimeSinceLastPrint = 0.0f;

		while (CurrentTime < 30.0f)
		{
			UnrealAudioModuleSystem->Update();

			// Randomly load a new file
			if (FMath::FRandRange(0.0f, 100.0f) < 10.0f)
			{
				// Randomly pick a file from the list of input files
				int Index = FMath::RandRange(0, Files.Num() - 1);
				FString& Path = Files[Index];

				// Load that baby up
				TSharedPtr<ISoundFile> SoundFile;
				if (FMath::RandBool())
				{
					SoundFile = UnrealAudioModuleSystem->LoadSoundFile(*Path);
				}
				else
				{
					SoundFile = UnrealAudioModuleSystem->StreamSoundFile(*Path);
				}

				// Try to store the new loaded file in a previously used slot..
				bool bFoundEmptySlot = false;
				for (int32 i = 0; i < LoadedFiles.Num(); ++i)
				{
					if (!LoadedFiles[i].IsValid())
					{
						LoadedFiles[i] = SoundFile;
						bFoundEmptySlot = true;
						break;
					}
				}

				// Otherwise add it to our list of loaded files
				if (!bFoundEmptySlot)
				{
					LoadedFiles.Add(SoundFile);
				}
			}

			// Loop through the loaded files and randomly unref them
			// which will cause them to become "inactive" if nobody else is refing them.
			// This will give the sound file manager a chance to flush the asset over time
			// or if the memory threshold is reached.
			for (int i = 0; i < LoadedFiles.Num(); ++i)
			{
				if (LoadedFiles[i].IsValid() && FMath::FRandRange(0.0f, 100.0f) < 5.0f)
				{
					LoadedFiles[i] = nullptr;
				}
			}

			if (CurrentTime - TimeSinceLastPrint >= 1.0f)
			{
				UnrealAudioModuleSystem->LogSoundFileMemoryInfo();
				TimeSinceLastPrint = CurrentTime;
			}

			CurrentTime += 0.033f;
			FPlatformProcess::Sleep(0.033f);
		}

		return true;
	}


	bool TestSoundFileManager(const FString& FolderOrPath)
	{
		if (FPaths::DirectoryExists(FolderOrPath))
		{
			// Test the first file
			TArray<FString> SoundFiles;
			FString Directory = FolderOrPath;
			GetSoundFileListInDirectory(Directory, SoundFiles);

			if (SoundFiles.Num() > 0)
			{
				return TestArrayOfFilesLoading(SoundFiles);
			}
			else
			{
				UE_LOG(LogUnrealAudio, Error, TEXT("Failed to find any sound files at directory %s"), *FolderOrPath);
				return false;
			}
		}
		else
		{
			return TestSingleFileLoad(FolderOrPath);
		}
	}

}

#endif // #if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS
