// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "ExportDialogueScriptCommandlet.generated.h"

class UDialogueWave;
struct FDialogueContextMapping;

USTRUCT()
struct FDialogueScriptEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString SpeakingVoice;
	
	UPROPERTY()
	TArray<FString> TargetVoices;

	UPROPERTY()
	FString SpokenDialogue;

	UPROPERTY()
	FString VoiceActorDirection;

	UPROPERTY()
	FString AudioFileName;

	UPROPERTY()
	FString DialogueAsset;

	UPROPERTY()
	bool IsRecorded;

	UPROPERTY()
	TArray<FString> LocalizationKeys;

	UPROPERTY()
	FString SpeakingVoiceGUID;

	UPROPERTY()
	TArray<FString> TargetVoiceGUIDs;

	UPROPERTY()
	FString DialogueAssetGUID;
};

/**
 *	UExportDialogueScriptCommandlet: Handles exporting localized script sheets for dialogue wave assets.
 */
UCLASS()
class UExportDialogueScriptCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

private:
	static FString GenerateCSVHeader();
	static FString GenerateCSVRow(const FDialogueScriptEntry& InDialogueScriptEntry);

	static void PopulateDialogueScriptEntry(const UDialogueWave* InDialogueWave, const UDialogueWave* InLocalizedDialogueWave, const FDialogueContextMapping& InPrimaryContext, const TArray<const FDialogueContextMapping*>& InAdditionalContexts, const FString& InLocalizedDialogue, const FString& InLocalizedVoiceActorDirection, FDialogueScriptEntry& OutDialogueScriptEntry);
};
