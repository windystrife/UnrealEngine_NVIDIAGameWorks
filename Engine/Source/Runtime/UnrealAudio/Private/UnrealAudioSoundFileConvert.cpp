// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioPrivate.h"
#include "Modules/ModuleManager.h"

#if ENABLE_UNREAL_AUDIO


#define SOUND_CONVERT_CHECK(Result)									\
	if (Result != ESoundFileError::NONE)							\
	{																\
		OnError(Result);											\
		return;														\
	}


namespace UAudio
{
	class FAsyncSoundFileConvertTask
	{
	public:
		FAsyncSoundFileConvertTask(FUnrealAudioModule* InAudioModule, const FString* InSoundFilePath, const FString* OutSoundFilePath, const FSoundFileConvertFormat* InConvertFormat)
			: AudioModule(InAudioModule)
			, SoundFilePath(*InSoundFilePath)
			, OutSoundFilePath(*OutSoundFilePath)
			, ConvertFormat(*InConvertFormat)
		{
			AudioModule->IncrementBackgroundTaskCount();
		}
		
		~FAsyncSoundFileConvertTask()
		{
			AudioModule->DecrementBackgroundTaskCount();
		}

		void DoWork()
		{
			// Synchronously load the input sound file
			TSharedPtr<ISoundFile> InputSoundFile = AudioModule->LoadSoundFile(*SoundFilePath, false);
			if (!InputSoundFile.IsValid())
			{
				return;
			}

			TSharedPtr<FSoundFileReader> InputSoundFileReader = AudioModule->CreateSoundFileReader();

			ESoundFileError::Type Error = InputSoundFileReader->Init(InputSoundFile, false);
			if (Error != ESoundFileError::NONE)
			{
				return;
			}
			check(Error == ESoundFileError::NONE);

			// Cast to an internal object to get access to non-public API
			FSoundFile* InputSoundFileInternal = static_cast<FSoundFile*>(InputSoundFile.Get());

			FSoundFileDescription InputDescription;
			Error = InputSoundFileInternal->GetDescription(InputDescription);
			check(Error == ESoundFileError::NONE);

			TArray<ESoundFileChannelMap::Type> ChannelMap;
			Error = InputSoundFileInternal->GetChannelMap(ChannelMap);
			check(Error == ESoundFileError::NONE);

			FSoundFileDescription NewSoundFileDescription;
			NewSoundFileDescription.NumChannels = InputDescription.NumChannels;
			NewSoundFileDescription.NumFrames	= InputDescription.NumFrames;
			NewSoundFileDescription.FormatFlags = ConvertFormat.Format;
			NewSoundFileDescription.SampleRate	= ConvertFormat.SampleRate;
			NewSoundFileDescription.NumSections = InputDescription.NumSections;
			NewSoundFileDescription.bIsSeekable = InputDescription.bIsSeekable;

			TSharedPtr<FSoundFileWriter> SoundFileWriter = AudioModule->CreateSoundFileWriter();
			Error = SoundFileWriter->Init(NewSoundFileDescription, ChannelMap, ConvertFormat.EncodingQuality);
			check(Error == ESoundFileError::NONE);

			// Create a buffer to do the processing 
			SoundFileCount ProcessBufferSamples = 1024 * NewSoundFileDescription.NumChannels;
			TArray<float> ProcessBuffer;
			ProcessBuffer.Init(0.0f, ProcessBufferSamples);

			double SampleRateConversionRatio = (double)InputDescription.SampleRate / ConvertFormat.SampleRate;

			FSampleRateConverter SampleRateConverter;
			SampleRateConverter.Init(SampleRateConversionRatio, NewSoundFileDescription.NumChannels);

			TArray<float> OutputBuffer;
			SoundFileCount OutputBufferSamples = ProcessBufferSamples / SampleRateConversionRatio;
			OutputBuffer.Reserve(OutputBufferSamples);

			// Find the max value if we've been told to do peak normalization on import
			float MaxValue = 0.0f;
			SoundFileCount InputSamplesRead = 0;
			bool bPerformPeakNormalization = ConvertFormat.bPerformPeakNormalization;
			if (bPerformPeakNormalization)
			{
				Error = InputSoundFileReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
				SOUND_CONVERT_CHECK(Error);

				while (InputSamplesRead)
				{
					for (SoundFileCount Sample = 0; Sample < InputSamplesRead; ++Sample)
					{
						if (ProcessBuffer[Sample] > FMath::Abs(MaxValue))
						{
							MaxValue = ProcessBuffer[Sample];
						}
					}

					Error = InputSoundFileReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
					SOUND_CONVERT_CHECK(Error);
				}

				// If this happens, it means we have a totally silent file
				if (MaxValue == 0.0)
				{
					bPerformPeakNormalization = false;
				}

				// Seek the file back to the beginning
				SoundFileCount OutOffset;
				InputSoundFileReader->SeekFrames(0, ESoundFileSeekMode::FROM_START, OutOffset);
			}

			bool SamplesProcessed = true;

			// Read the first block of samples
			Error = InputSoundFileReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
			SOUND_CONVERT_CHECK(Error);

			while (InputSamplesRead != 0)
			{
				SampleRateConverter.ProcessBlock(ProcessBuffer.GetData(), InputSamplesRead, OutputBuffer);

				SoundFileCount SamplesWritten;
				Error = SoundFileWriter->WriteSamples((const float*)OutputBuffer.GetData(), OutputBuffer.Num(), SamplesWritten);
				SOUND_CONVERT_CHECK(Error);
				DEBUG_AUDIO_CHECK(SamplesWritten == OutputBuffer.Num());

				OutputBuffer.Reset();

				// read more samples
				Error = InputSoundFileReader->ReadSamples(ProcessBuffer.GetData(), ProcessBufferSamples, InputSamplesRead);
				SOUND_CONVERT_CHECK(Error);

				// ... normalize the samples if we're told to
				if (bPerformPeakNormalization)
				{
					for (int32 Sample = 0; Sample < InputSamplesRead; ++Sample)
					{
						ProcessBuffer[Sample] /= MaxValue;
					}
				}
			}

			// Release the sound file handles as soon as we finished converting the file
			InputSoundFileReader->Release();
			SoundFileWriter->Release();

			// Get the raw binary data
			TArray<uint8>* Data = nullptr;
			SoundFileWriter->GetData(&Data);

			// Write the data to the output file
			bool bSuccess = FFileHelper::SaveArrayToFile(*Data, *OutSoundFilePath);
			check(bSuccess);
		}


		bool CanAbandon()
		{ 
			return true; 
		}
		
		void Abandon() 
		{
		}

		void OnError(ESoundFileError::Type Error)
		{
//			((ISoundFileInternal*)SoundFile.Get())->SetError(Error);
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncSoundFileConvertTask, STATGROUP_ThreadPoolAsyncTasks);
		}

		FUnrealAudioModule* AudioModule;
		FString SoundFilePath;
		FString OutSoundFilePath;
		FSoundFileConvertFormat ConvertFormat;
		friend class FAsyncTask<FAsyncSoundFileConvertTask>;
	};

	void FUnrealAudioModule::ConvertSound(const FString& InputFilePath, const FString& OutputFilePath, const FSoundFileConvertFormat& ConvertFormat)
	{
		// Make an auto-deleting task to perform the sound file loading work in the background task thread pool
		FAutoDeleteAsyncTask<FAsyncSoundFileConvertTask>* Task = new FAutoDeleteAsyncTask<FAsyncSoundFileConvertTask>(this, &InputFilePath, &OutputFilePath, &ConvertFormat);
		Task->StartBackgroundTask();
	} //-V773

	void FUnrealAudioModule::ConvertSound(const FString& InputFilePath, const FString& OutputFilePath)
	{
		// Make an auto-deleting task to perform the sound file loading work in the background task thread pool
		FAutoDeleteAsyncTask<FAsyncSoundFileConvertTask>* Task = new FAutoDeleteAsyncTask<FAsyncSoundFileConvertTask>(this, &InputFilePath, &OutputFilePath, &DefaultConvertFormat);
		Task->StartBackgroundTask();
	} //-V773

}

#endif // #if ENABLE_UNREAL_AUDIO

