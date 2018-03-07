// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlayerInput.cpp: Unreal input system.
=============================================================================*/

#include "Sound/AudioSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundNodeQualityLevel.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AudioSettings"

FAudioPlatformSettings FAudioPlatformSettings::GetPlatformSettings(const TCHAR* PlatformSettingsConfigFile)
{
	FAudioPlatformSettings Settings;

	FString TempString;

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioSampleRate"), TempString, GEngineIni))
	{
		Settings.SampleRate = FMath::Max(FCString::Atoi(*TempString), 8000);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioCallbackBufferFrameSize"), TempString, GEngineIni))
	{
		Settings.CallbackBufferFrameSize = FMath::Max(FCString::Atoi(*TempString), 256);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumBuffersToEnqueue"), TempString, GEngineIni))
	{
		Settings.NumBuffers = FMath::Max(FCString::Atoi(*TempString), 1);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioMaxChannels"), TempString, GEngineIni))
	{
		Settings.MaxChannels = FMath::Max(FCString::Atoi(*TempString), 0);
	}

	if (GConfig->GetString(PlatformSettingsConfigFile, TEXT("AudioNumSourceWorkers"), TempString, GEngineIni))
	{
		Settings.NumSourceWorkers = FMath::Max(FCString::Atoi(*TempString), 0);
	}

	return Settings;
}

UAudioSettings::UAudioSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SectionName = TEXT("Audio");
	AddDefaultSettings();

	bAllowVirtualizedSounds = true;
	bIsAudioMixerEnabled = false;
}

void UAudioSettings::AddDefaultSettings()
{
	FAudioQualitySettings DefaultSettings;
	DefaultSettings.DisplayName = LOCTEXT("DefaultSettingsName", "Default");
	GConfig->GetInt(TEXT("Audio"), TEXT("MaxChannels"), DefaultSettings.MaxChannels, GEngineIni); // for backwards compatibility
	QualityLevels.Add(DefaultSettings);
	bAllowVirtualizedSounds = true;
	DefaultReverbSendLevel = 0.2f;
}

#if WITH_EDITOR
void UAudioSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Cache at least the first entry in case someone tries to clear the array
	CachedQualityLevels = QualityLevels;
}

void UAudioSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		bool bReconcileNodes = false;

		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAudioSettings, QualityLevels))
		{
			if (QualityLevels.Num() == 0)
			{
				QualityLevels.Add(CachedQualityLevels[0]);
			}
			else if (QualityLevels.Num() > CachedQualityLevels.Num())
			{
				for (FAudioQualitySettings& AQSettings : QualityLevels)
				{
					if (AQSettings.DisplayName.IsEmpty())
					{
						bool bFoundDuplicate;
						int32 NewQualityLevelIndex = 0;
						FText NewLevelName;
						do 
						{
							bFoundDuplicate = false;
							NewLevelName = FText::Format(LOCTEXT("NewQualityLevelName","New Level{0}"), (NewQualityLevelIndex > 0 ? FText::FromString(FString::Printf(TEXT(" %d"),NewQualityLevelIndex)) : FText::GetEmpty()));
							for (const FAudioQualitySettings& QualityLevelSettings : QualityLevels)
							{
								if (QualityLevelSettings.DisplayName.EqualTo(NewLevelName))
								{
									bFoundDuplicate = true;
									break;
								}
							}
							NewQualityLevelIndex++;
						} while (bFoundDuplicate);
						AQSettings.DisplayName = NewLevelName;
					}
				}
			}

			bReconcileNodes = true;
		}
		else if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAudioQualitySettings, DisplayName))
		{
			bReconcileNodes = true;
		}

		if (bReconcileNodes)
		{
			for (TObjectIterator<USoundNodeQualityLevel> It; It; ++It)
			{
				It->ReconcileNode(true);
			}
		}
	}
}
#endif

const FAudioQualitySettings& UAudioSettings::GetQualityLevelSettings(int32 QualityLevel) const
{
	check(QualityLevels.Num() > 0);
	return QualityLevels[FMath::Clamp(QualityLevel, 0, QualityLevels.Num() - 1)];
}

void UAudioSettings::SetAudioMixerEnabled(const bool bInAudioMixerEnabled)
{
	bIsAudioMixerEnabled = bInAudioMixerEnabled;
}

const bool UAudioSettings::IsAudioMixerEnabled() const
{
	return bIsAudioMixerEnabled;
}

int32 UAudioSettings::GetHighestMaxChannels() const
{
	check(QualityLevels.Num() > 0);
	
	int32 HighestMaxChannels = -1;
	for (const FAudioQualitySettings& Settings : QualityLevels)
	{
		if (Settings.MaxChannels > HighestMaxChannels)
		{
			HighestMaxChannels = Settings.MaxChannels;
		}
	}

	return HighestMaxChannels;
}

#undef LOCTEXT_NAMESPACE
