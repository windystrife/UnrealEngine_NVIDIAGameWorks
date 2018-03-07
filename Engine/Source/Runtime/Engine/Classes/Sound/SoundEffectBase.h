// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "HAL/ThreadSafeBool.h"
#include "Sound/SoundEffectPreset.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_SWITCH
// Switch uses page alignment for submitted buffers
#define AUDIO_BUFFER_ALIGNMENT 4096
#else
#define AUDIO_BUFFER_ALIGNMENT 16
#endif

namespace Audio
{
	typedef TArray<float, TAlignedHeapAllocator<AUDIO_BUFFER_ALIGNMENT>> AlignedFloatBuffer;
	typedef TArray<uint8, TAlignedHeapAllocator<AUDIO_BUFFER_ALIGNMENT>> AlignedByteBuffer;
}


// The following macro code creates boiler-plate code for a sound effect preset and hides unnecessary details from user-created effects.

// Macro chain to expand "MyEffectName" to "FMyEffectNameSettings"
#define EFFECT_SETTINGS_NAME2(CLASS_NAME, SUFFIX) F ## CLASS_NAME ## SUFFIX
#define EFFECT_SETTINGS_NAME1(CLASS_NAME, SUFFIX) EFFECT_SETTINGS_NAME2(CLASS_NAME, SUFFIX)
#define EFFECT_SETTINGS_NAME(CLASS_NAME)		  EFFECT_SETTINGS_NAME1(CLASS_NAME, Settings)

#define EFFECT_PRESET_NAME2(CLASS_NAME, SUFFIX)  U ## CLASS_NAME ## SUFFIX
#define EFFECT_PRESET_NAME1(CLASS_NAME, SUFFIX)  EFFECT_PRESET_NAME2(CLASS_NAME, SUFFIX)
#define EFFECT_PRESET_NAME(CLASS_NAME)			 EFFECT_PRESET_NAME1(CLASS_NAME, Preset)

#define GET_EFFECT_SETTINGS(EFFECT_NAME) \
		U##EFFECT_NAME##Preset* _Preset = CastChecked<U##EFFECT_NAME##Preset>(Preset); \
		F##EFFECT_NAME##Settings Settings = _Preset->GetSettings(); \
	
#define EFFECT_PRESET_METHODS(EFFECT_NAME) \
		virtual FText GetAssetActionName() const override { return FText::FromString(#EFFECT_NAME); } \
		virtual UClass* GetSupportedClass() const override { return EFFECT_PRESET_NAME(EFFECT_NAME)::StaticClass(); } \
		virtual FSoundEffectBase* CreateNewEffect() const override { return new F##EFFECT_NAME; } \
		virtual USoundEffectPreset* CreateNewPreset(UObject* InParent, FName Name, EObjectFlags Flags) const override \
		{ \
			USoundEffectPreset* NewPreset = NewObject<EFFECT_PRESET_NAME(EFFECT_NAME)>(InParent, GetSupportedClass(), Name, Flags); \
			NewPreset->Init(); \
			return NewPreset; \
		} \
		virtual void Init() override \
		{ \
			FScopeLock ScopeLock(&SettingsCritSect); \
			SettingsCopy = Settings; \
		} \
		void UpdateSettings(const F##EFFECT_NAME##Settings& InSettings) \
		{ \
			FScopeLock ScopeLock(&SettingsCritSect); \
			SettingsCopy = InSettings; \
			Update(); \
		} \
		F##EFFECT_NAME##Settings GetSettings() \
		{ \
			FScopeLock ScopeLock(&SettingsCritSect); \
			return SettingsCopy; \
		} \
		FCriticalSection SettingsCritSect; \
		F##EFFECT_NAME##Settings SettingsCopy; \


#define EFFECT_PRESET_METHODS_NO_ASSET_ACTIONS(EFFECT_NAME) \
		virtual bool HasAssetActions() const override { return false; } \
		EFFECT_PRESET_METHODS(EFFECT_NAME)

class USoundEffectPreset;

class ENGINE_API FSoundEffectBase
{
public:
	FSoundEffectBase();

	virtual ~FSoundEffectBase();

	/** Called when the sound effect's preset changed. */
	virtual void OnPresetChanged() {};

	/** Returns if the submix is active or bypassing audio. */
	bool IsActive() const;

	/** Enables the submix effect. */
	void SetEnabled(const bool bInIsEnabled);

	/** Updates preset on audio render thread. */
	void Update();

	void SetPreset(USoundEffectPreset* Inpreset);

	/** Registers the parent preset and registers this instance with the preset. */
	void RegisterWithPreset(USoundEffectPreset* InParentPreset);

	/** Removes the instance from the preset. */
	void UnregisterWithPreset();

	/** Queries if the given preset object is the parent preset, i.e. the preset which spawned this effect instance. */
	bool IsParentPreset(USoundEffectPreset* InPreset) const;

	/** Enqueues a lambda command on a thread safe queue which is pumped from the audio render thread. */
	void EffectCommand(TFunction<void()> Command);

protected:

	void PumpPendingMessages();

	FCriticalSection SettingsCritSect;
	TArray<uint8> CurrentAudioThreadSettingsData;

	FThreadSafeBool bChanged;
	USoundEffectPreset* Preset;
	USoundEffectPreset* ParentPreset;

	FThreadSafeBool bIsRunning;
	FThreadSafeBool bIsActive;

	// Effect commmand queue
	TQueue<TFunction<void()>> CommandQueue;

	// Allow FAudioMixerSubmix to call ProcessAudio
	friend class FMixerSubmix;

};

