// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioCapture.h"
#include "ISequenceRecorder.h"
#include "ISequenceAudioRecorder.h"
#include "ModuleManager.h"
#include "AudioRecordingManager.h"

class FAudioRecorder : public ISequenceAudioRecorder
{
public:

	virtual void Start(const FAudioRecorderSettings& Settings) override
	{
		FAudioRecordingManager::Get().StartRecording(Settings.Directory, Settings.AssetName, Settings.RecordingDurationSec, Settings.GainDB, Settings.InputBufferSize);
	}

	virtual USoundWave* Stop() override
	{
		return FAudioRecordingManager::Get().StopRecording();
	}
};


class FAudioCaptureModule
	: public IModuleInterface
{
	virtual void StartupModule() override
	{
		ISequenceRecorder& Recorder = FModuleManager::Get().LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
		RecorderHandle = Recorder.RegisterAudioRecorder(
			[](){
				return TUniquePtr<ISequenceAudioRecorder>(new FAudioRecorder());
			}
		);
	}

	virtual void ShutdownModule() override
	{
		ISequenceRecorder* Recorder = FModuleManager::Get().GetModulePtr<ISequenceRecorder>("SequenceRecorder");
		
		if (Recorder)
		{
			Recorder->UnregisterAudioRecorder(RecorderHandle);
		}
	}

	FDelegateHandle RecorderHandle;
};


IMPLEMENT_MODULE( FAudioCaptureModule, AudioCapture );
