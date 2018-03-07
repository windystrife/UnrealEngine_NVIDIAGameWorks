// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "NetworkSettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Network settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Network"))
class ENGINE_API UNetworkSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	//! Default MaxRepArraySize @see MaxRepArraySize.
	static const int32 DefaultMaxRepArraySize = 2 * 1024;

	//! Default MaxRepArrayMemory @see MaxRepArrayMemory.
	static const int32 DefaultMaxRepArrayMemory = UINT16_MAX;

	UPROPERTY(config, EditAnywhere, Category=libcurl, meta=(
		ConsoleVariable="n.VerifyPeer",DisplayName="Verify Peer",
		ToolTip="If true, libcurl authenticates the peer's certificate. Disable to allow self-signed certificates."))
	uint32 bVerifyPeer:1;

	UPROPERTY(config, EditAnywhere, Category=World, meta = (
		ConsoleVariable = "p.EnableMultiplayerWorldOriginRebasing", DisplayName = "Enable Multiplayer World Origin Rebasing",
		ToolTip="If true, origin rebasing is enabled in multiplayer games, meaning that servers and clients can have different local world origins."))
	uint32 bEnableMultiplayerWorldOriginRebasing : 1;

	//! Maximum allowable size for replicated dynamic arrays (in number of elements)
	UPROPERTY(config, EditAnywhere, Category = replication, meta = (
		ConsoleVariable = "net.MaxRepArraySize", DisplayName = "Max Array Size",
		ToolTip = "Maximum allowable size for replicated dynamic arrays (in number of elements). Must be between 1 and 65535.",
		ClampMin = "1", ClampMax = "65535", UIMin = "1", UIMax = "65535"))
	int32 MaxRepArraySize = DefaultMaxRepArraySize;

	//! Maximum allowable size for replicated dynamic arrays (in bytes)
	UPROPERTY(config, EditAnywhere, Category = replication, meta = (
		ConsoleVariable = "net.MaxRepArrayMemory", DisplayName = "Max Array Memory",
		ToolTip = "Maximum allowable size for replicated dynamic arrays (in bytes).  Must be between 1 and 65535.",
		ClampMin = "1", ClampMax = "65535", UIMin = "1", UIMax = "65535"))
	int32 MaxRepArrayMemory = DefaultMaxRepArrayMemory;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface
};
