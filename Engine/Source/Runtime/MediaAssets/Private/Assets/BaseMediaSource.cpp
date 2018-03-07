// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BaseMediaSource.h"
#include "UObject/SequencerObjectVersion.h"

#if WITH_EDITOR
	#include "Interfaces/ITargetPlatform.h"
#endif


/* UObject interface
 *****************************************************************************/

void UBaseMediaSource::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	FString Url = GetUrl();

	if (!Url.IsEmpty())
	{
		OutTags.Add(FAssetRegistryTag("Url", Url, FAssetRegistryTag::TT_Alphabetical));
	}
}


#if WITH_EDITOR
void UBaseMediaSource::GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const
{
}
#endif


void UBaseMediaSource::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	auto CustomVersion = Ar.CustomVer(FSequencerObjectVersion::GUID);

	if (Ar.IsLoading() && (CustomVersion < FSequencerObjectVersion::RenameMediaSourcePlatformPlayers))
	{
#if WITH_EDITORONLY_DATA
		if (!Ar.IsFilterEditorOnly())
		{
			TMap<FString, FString> DummyPlatformPlayers;
			Ar << DummyPlatformPlayers;
		}
#endif

		FString DummyDefaultPlayer;
		Ar << DummyDefaultPlayer;
	}
	else
	{
#if WITH_EDITORONLY_DATA
		if (Ar.IsFilterEditorOnly())
		{
			if (Ar.IsSaving())
			{
				const FName* PlatformPlayerName = PlatformPlayerNames.Find(Ar.CookingTarget()->IniPlatformName());
				PlayerName = (PlatformPlayerName != nullptr) ? *PlatformPlayerName : NAME_None;
			}

			Ar << PlayerName;
		}
		else
		{
			Ar << PlatformPlayerNames;
		}
#else
		Ar << PlayerName;
#endif
	}
}


/* IMediaOptions interface
 *****************************************************************************/

FName UBaseMediaSource::GetDesiredPlayerName() const
{
#if WITH_EDITORONLY_DATA
	const FString RunningPlatformName(FPlatformProperties::IniPlatformName());
	const FName* PlatformPlayerName = PlatformPlayerNames.Find(RunningPlatformName);

	if (PlatformPlayerName == nullptr)
	{
		return Super::GetDesiredPlayerName();
	}

	return *PlatformPlayerName;
#else
	return PlayerName;
#endif
}
