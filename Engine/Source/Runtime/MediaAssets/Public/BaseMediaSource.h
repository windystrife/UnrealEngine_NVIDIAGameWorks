// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MediaSource.h"

#include "BaseMediaSource.generated.h"


/**
 * Base class for concrete media sources.
 */
UCLASS(Abstract, BlueprintType, hidecategories=(Object))
class MEDIAASSETS_API UBaseMediaSource
	: public UMediaSource
{
	GENERATED_BODY()

public:

#if WITH_EDITORONLY_DATA

	/** Override native media player plug-ins per platform (Empty = find one automatically). */
	UPROPERTY(transient, BlueprintReadWrite, EditAnywhere, Category=Platforms, Meta=(DisplayName="Player Overrides"))
	TMap<FString, FName> PlatformPlayerNames;

#endif

public:

	//~ UObject interface

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
#endif

public:

	//~ IMediaOptions interface

	virtual FName GetDesiredPlayerName() const override;

private:

	/** Name of the desired native media player (Empty = find one automatically). */
	UPROPERTY(transient)
	FName PlayerName;
};
