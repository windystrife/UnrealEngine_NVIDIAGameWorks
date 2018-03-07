// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoiceCapture.h"
#include "VoicePackage.h"
#include "Containers/Ticker.h"
#include "WindowsHWrapper.h"

typedef struct IDirectSound8 *LPDIRECTSOUND8;

class IVoiceCapture;
class FVoiceCaptureWindows;
struct FVoiceCaptureWindowsVars;

/**
 * DirectSound wrapper for initialization / shutdown
 */ 
class FVoiceCaptureDeviceWindows
{
private:

    /** One instance of the direct sound API */
	static FVoiceCaptureDeviceWindows* Singleton;
	/** All outstanding voice capture objects */
	TArray<IVoiceCapture*> ActiveVoiceCaptures;
	/** Is DirectSound setup correctly */
	bool bInitialized;

    /** Initialize DirectSound and the voice capture device */
	bool Init();
    /** Shutdown DirectSound and the voice capture device */
	void Shutdown();

PACKAGE_SCOPE:

	/**
	 * Create the device singleton 
	 *
	 * @return singleton capable of providing voice capture features
	 */
	static FVoiceCaptureDeviceWindows* Create();
	/**
	 * Destroy the device singleton
	 */
	static void Destroy();
	/**
	 * Free a voice capture object created by CreateVoiceCaptureObject()
	 *
	 * @param VoiceCaptureObj voice capture object to free
	 */
	void FreeVoiceCaptureObject(IVoiceCapture* VoiceCaptureObj);

public:

	struct FCaptureDeviceInfo
	{
		/** Enumerated capture device name */
		FString DeviceName;
		/** Enumerated capture device guid */
		GUID DeviceId;
	};

	/** DirectSound8 Interface */
	LPDIRECTSOUND8 DirectSound;

	/** HMD audio input device to use */
	FString HMDAudioInputDevice;
	/** GUID of selected voice capture device */
	FCaptureDeviceInfo DefaultVoiceCaptureDevice;

	/** All available capture devices */
	TMap<FString, FCaptureDeviceInfo> Devices;

	/** Constructors */
	FVoiceCaptureDeviceWindows();
	~FVoiceCaptureDeviceWindows();

	/**
	 * Create a single voice capture buffer
	 *
	 * @param DeviceName name of device to capture with, empty for default device
	 * @param SampleRate valid sample rate to capture audio data (8khz - 48khz)
	 * @param NumChannel number of audio channels (1 - mono, 2 - stereo)
	 *
	 * @return class capable of recording voice on a given device
	 */
	FVoiceCaptureWindows* CreateVoiceCaptureObject(const FString& DeviceName, int32 SampleRate, int32 NumChannels);

	/**
	 * Singleton accessor
	 * 
	 * @return access to the voice capture device
	 */
	static FVoiceCaptureDeviceWindows* Get();
};

/**
 * Windows implementation of voice capture using DirectSound
 */
class FVoiceCaptureWindows : public IVoiceCapture, public FTickerObjectBase
{
public:

	FVoiceCaptureWindows();
	~FVoiceCaptureWindows();

	// IVoiceCapture
	virtual bool Init(const FString& DeviceName, int32 SampleRate, int32 NumChannels) override;
	virtual void Shutdown() override;
	virtual bool Start() override;
	virtual void Stop() override;
	virtual bool ChangeDevice(const FString& DeviceName, int32 SampleRate, int32 NumChannels) override;
	virtual bool IsCapturing() override;
	virtual EVoiceCaptureState::Type GetCaptureState(uint32& OutAvailableVoiceData) const override;
	virtual EVoiceCaptureState::Type GetVoiceData(uint8* OutVoiceBuffer, uint32 InVoiceBufferSize, uint32& OutAvailableVoiceData) override;
	virtual int32 GetBufferSize() const override;
	virtual void DumpState() const override;

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

private:

	/** All windows related variables to hide windows includes */
	FVoiceCaptureWindowsVars* CV;
	/** Last time data was captured */
	double LastCaptureTime;
	/** State of capture device */
	EVoiceCaptureState::Type VoiceCaptureState;
	/** Uncompressed audio buffer */
	TArray<uint8> UncompressedAudioBuffer;

	/**
	 * Create the D3D8 capture device
	 *
	 * @param DeviceName name of device to capture with, empty for default device
	 * @param SampleRate valid sample rate to capture audio data (8khz - 48khz)
	 * @param NumChannel number of audio channels (1 - mono, 2 - stereo)
	 * 
	 * @return true if successful, false otherwise
	 */
	bool CreateCaptureBuffer(const FString& DeviceName, int32 SampleRate, int32 NumChannels);

	/** Clear the capture buffer and release all resources */
	void FreeCaptureBuffer();

	/**
	 * Lock the DirectSound audio buffer and copy out the available data based on a notification
	 * and the current audio buffer capture cursor
	 */
	void ProcessData();
	/**
	 * Create notifications on the voice capture buffer, evenly distributed
	 *
	 * @param BufferSize size of buffer created
	 */
	bool CreateNotifications(uint32 BufferSize);
};
