// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/PlatformFile.h"
#include "UnrealAudioTypes.h"
#include "Tests/UnrealAudioTestGenerators.h"
#include "UnrealAudioPrivate.h"

#if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS

namespace UAudio
{
	/** Static singleton test generator object. */
	static IUnrealAudioModule* UnrealAudioModule = nullptr;
	static Test::IGenerator* TestGenerator = nullptr;
	static FThreadSafeBool bTestActive = false;

	void FUnrealAudioModule::InitializeDeviceTests(IUnrealAudioModule* Module)
	{
		UnrealAudioModule = Module;
	}

	bool TestDeviceQuery()
	{
		UE_LOG(LogUnrealAudio, Display, TEXT("Running audio device query test..."));

		IUnrealAudioDeviceModule* UnrealAudioDevice = UnrealAudioModule->GetDeviceModule();

		if (!UnrealAudioDevice)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed: No Audio Device Object."));
			return false;
		}

		
		UE_LOG(LogUnrealAudio, Display, TEXT("Querying which audio device API we're using..."));
		EDeviceApi::Type DeviceApi;
		if (!UnrealAudioDevice->GetDevicePlatformApi(DeviceApi))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed."));
			return false;
		}

		const TCHAR* ApiName = EDeviceApi::ToString(DeviceApi);
		UE_LOG(LogUnrealAudio, Display, TEXT("Success: Using %s"), ApiName);

		if (DeviceApi == EDeviceApi::DUMMY)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("This is a dummy API. Platform not implemented yet."));
			return true;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Querying the number of output devices for current system..."));
		uint32 NumOutputDevices = 0;
		if (!UnrealAudioDevice->GetNumOutputDevices(NumOutputDevices))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Failed."));
			return false;
		}
		UE_LOG(LogUnrealAudio, Display, TEXT("Success: %d Output Devices Found"), NumOutputDevices);

		UE_LOG(LogUnrealAudio, Display, TEXT("Retrieving output device info for each output device..."));
		for (uint32 DeviceIndex = 0; DeviceIndex < NumOutputDevices; ++DeviceIndex)
		{
			FDeviceInfo DeviceInfo;
			if (!UnrealAudioDevice->GetOutputDeviceInfo(DeviceIndex, DeviceInfo))
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("Failed for index %d."), DeviceIndex);
				return false;
			}

			UE_LOG(LogUnrealAudio, Display, TEXT("Device Index: %d"), DeviceIndex);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Name: %s"), *DeviceInfo.FriendlyName);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device FrameRate: %d"), DeviceInfo.FrameRate);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device NumChannels: %d"), DeviceInfo.NumChannels);
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Default?: %s"), DeviceInfo.bIsSystemDefault ? TEXT("yes") : TEXT("no"));
			UE_LOG(LogUnrealAudio, Display, TEXT("Device Native Format: %s"), EStreamFormat::ToString(DeviceInfo.StreamFormat));

			UE_LOG(LogUnrealAudio, Display, TEXT("Speaker Array:"));

			for (int32 Channel = 0; Channel < DeviceInfo.Speakers.Num(); ++Channel)
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("    %s"), ESpeaker::ToString(DeviceInfo.Speakers[Channel]));
			}
		}

		return true;
	}

	bool FUnrealAudioModule::DeviceTestCallback(struct FCallbackInfo& CallbackInfo)
	{
		if (!bTestActive)
		{
			return true;
		}

		check(TestGenerator);

		static int32 CurrentSecond = -1;
		int32 StreamSecond = (int32)CallbackInfo.StreamTime;
		if (StreamSecond != CurrentSecond)
		{
			if (CurrentSecond == -1)
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("Stream Time (seconds):"));
			}
			CurrentSecond = StreamSecond;
			UE_LOG(LogUnrealAudio, Display, TEXT("%d"), CurrentSecond);
		}

		// Sets any data used by lots of different generators in static data struct
		Test::UpdateCallbackData(CallbackInfo);

		return TestGenerator->GetNextBuffer(CallbackInfo);
	}

	static bool DoOutputTest(const FString& TestName, double LifeTime, Test::IGenerator* Generator)
	{
		check(Generator);
		check(TestGenerator == nullptr);
		check(bTestActive == false);

		UE_LOG(LogUnrealAudio, Display, TEXT("Running audio test: \"%s\"..."), *TestName);

		if (LifeTime > 0.0)
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Time of test: %d (seconds)"), LifeTime);
		}
		else
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Time of test: (indefinite)"));
		}

		TestGenerator = Generator;
		bTestActive = true;

		// Block this thread until the synthesizer is done
		while (!Generator->IsDone())
		{
			FPlatformProcess::Sleep(1);
		}

		bTestActive = false;
		TestGenerator = nullptr;

		// At this point audio device I/O is done
		UE_LOG(LogUnrealAudio, Display, TEXT("Success!"));
		return true;
	}

	bool TestDeviceOutputSimple(double LifeTime)
	{
		Test::FSimpleOutput SimpleOutput(LifeTime);
		return DoOutputTest("output simple test", LifeTime, &SimpleOutput);
	}

	bool TestDeviceOutputRandomizedFm(double LifeTime)
	{
		Test::FPhaseModulator RandomizedFmGenerator(LifeTime);
		return DoOutputTest("output randomized FM synthesis", LifeTime, &RandomizedFmGenerator);
	}

	bool TestDeviceOutputNoisePan(double LifeTime)
	{
		Test::FNoisePan SimpleWhiteNoisePan(LifeTime);
		return DoOutputTest("output white noise panner", LifeTime, &SimpleWhiteNoisePan);
	}

	bool TestSourceConvert(const FString& FilePath, const FSoundFileConvertFormat& ConvertFormat)
	{
		//  Check if the path is a folder or a single file
		if (FPaths::DirectoryExists(FilePath))
		{
			// Test the first file
			TArray<FString> SoundFiles;
			FString Directory = FilePath;
			GetSoundFileListInDirectory(Directory, SoundFiles);

			FString FolderPath = FilePath;
			UE_LOG(LogUnrealAudio, Display, TEXT("Testing import then export of all audio files in directory: %s."), *FilePath);

			// Create the output exported directory if it doesn't exist
			FString OutputDir = FolderPath / "Exported";
			if (!FPaths::DirectoryExists(OutputDir))
			{
				FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*OutputDir);
			}

			FString SoundFileExtension;
			if (!GetFileExtensionForFormatFlags(ConvertFormat.Format, SoundFileExtension))
			{
				UE_LOG(LogUnrealAudio, Error, TEXT("Unknown sound file format."));
				return false;
			}

			// Convert and export all the files!
			UE_LOG(LogUnrealAudio, Display, TEXT("Converting and exporting..."));
			for (FString& InputFile : SoundFiles)
			{
				UE_LOG(LogUnrealAudio, Display, TEXT("%s"), *InputFile);

				// We're building up a filename with [NAME]_exported.[EXT] in the output directory
				FString BaseSoundFileName = FPaths::GetBaseFilename(InputFile);
				FString OutputFileName = BaseSoundFileName + TEXT("_exported.") + SoundFileExtension;
				FString OutputPath = OutputDir / OutputFileName;

				while (UnrealAudioModule->GetNumBackgroundTasks() > 2)
				{
					FPlatformProcess::Sleep(0.001f);
				}
				UnrealAudioModule->ConvertSound(InputFile, OutputPath, ConvertFormat);
			}
			return true;
		}
		else if (FPaths::FileExists(FilePath))
		{
			UE_LOG(LogUnrealAudio, Display, TEXT("Testing import, then export of a single sound source: %s."), *FilePath);
			UE_LOG(LogUnrealAudio, Display, TEXT("Convert Format: %s"), *FString::Printf(TEXT("%s - %s"), ESoundFileFormat::ToStringMajor(ConvertFormat.Format), ESoundFileFormat::ToStringMinor(ConvertFormat.Format)));
			UE_LOG(LogUnrealAudio, Display, TEXT("Convert SampleRate: %d"), ConvertFormat.SampleRate);
			UE_LOG(LogUnrealAudio, Display, TEXT("Convert EncodingQuality: %.2f"), ConvertFormat.EncodingQuality);
			UE_LOG(LogUnrealAudio, Display, TEXT("Perform Peak Normalization: %s"), ConvertFormat.bPerformPeakNormalization ? TEXT("Yes") : TEXT("No"));

			// Now setup the export path
			FString BaseSoundFileName = FPaths::GetBaseFilename(FilePath);
			FString SoundFileExtension = FPaths::GetExtension(FilePath);
			FString SoundFileDir = FPaths::GetPath(FilePath);

			// Create the export directory if it doesn't exist
			FString OutputDir = SoundFileDir;
			if (!FPaths::DirectoryExists(OutputDir))
			{
				FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*OutputDir);
			}

			// Append _exported to the file path just to make it clear that this is the exported version of the file
			FString OutputPath = SoundFileDir / (BaseSoundFileName + TEXT("_exported.") + SoundFileExtension);

			UnrealAudioModule->ConvertSound(FilePath, OutputPath, ConvertFormat);

			return true;
		}

		UE_LOG(LogUnrealAudio, Display, TEXT("Path %s is not a single file or a directory."), *FilePath);
		return false;
	}

}

#endif // #if ENABLE_UNREAL_AUDIO & WITH_DEV_AUTOMATION_TESTS
