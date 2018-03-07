// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "GameplayDebuggerConfig.generated.h"

struct FGameplayDebuggerInputModifier;

UENUM()
enum class EGameplayDebuggerOverrideMode : uint8
{
	Enable,
	Disable,
	UseDefault,
};

USTRUCT()
struct FGameplayDebuggerInputConfig
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = Input)
	FString ConfigName;

	UPROPERTY(EditAnywhere, Category = Input)
	FKey Key;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bModShift : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bModCtrl : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bModAlt : 1;

	UPROPERTY(EditAnywhere, Category = Input)
	uint32 bModCmd : 1;
};

USTRUCT()
struct FGameplayDebuggerCategoryConfig
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = Settings)
	FString CategoryName;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "bOverrideSlotIdx", ClampMin = -1, ClampMax = 9, UIMin = -1, UIMax = 9))
	int32 SlotIdx;

	UPROPERTY(EditAnywhere, Category = Settings)
	EGameplayDebuggerOverrideMode ActiveInGame;

	UPROPERTY(EditAnywhere, Category = Settings)
	EGameplayDebuggerOverrideMode ActiveInSimulate;

	UPROPERTY(EditAnywhere, Category = Settings)
	EGameplayDebuggerOverrideMode Hidden;

	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	uint32 bOverrideSlotIdx : 1;

	UPROPERTY(EditAnywhere, Category = Settings, EditFixedSize)
	TArray<FGameplayDebuggerInputConfig> InputHandlers;

	FGameplayDebuggerCategoryConfig() :
		ActiveInGame(EGameplayDebuggerOverrideMode::UseDefault), ActiveInSimulate(EGameplayDebuggerOverrideMode::UseDefault),
		Hidden(EGameplayDebuggerOverrideMode::UseDefault), bOverrideSlotIdx(false)
	{}
};

USTRUCT()
struct FGameplayDebuggerExtensionConfig
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, Category = Settings)
	FString ExtensionName;

	UPROPERTY(EditAnywhere, Category = Settings)
	EGameplayDebuggerOverrideMode UseExtension;

	UPROPERTY(EditAnywhere, Category = Settings, EditFixedSize)
	TArray<FGameplayDebuggerInputConfig> InputHandlers;

	FGameplayDebuggerExtensionConfig() : UseExtension(EGameplayDebuggerOverrideMode::UseDefault) {}
};

UCLASS(config = Engine, defaultconfig)
class GAMEPLAYDEBUGGER_API UGameplayDebuggerConfig : public UObject
{
	GENERATED_UCLASS_BODY()

	/** key used to activate visual debugger tool */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey ActivationKey;

	/** select next category row */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategoryRowNextKey;

	/** select previous category row */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategoryRowPrevKey;

	/** select category slot 0 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot0;

	/** select category slot 1 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot1;

	/** select category slot 2 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot2;

	/** select category slot 3 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot3;

	/** select category slot 4 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot4;

	/** select category slot 5 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot5;

	/** select category slot 6 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot6;

	/** select category slot 7 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot7;

	/** select category slot 8 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot8;

	/** select category slot 9 */
	UPROPERTY(config, EditAnywhere, Category = Input)
	FKey CategorySlot9;

	/** additional canvas padding: left */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingLeft;

	/** additional canvas padding: right */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingRight;

	/** additional canvas padding: top */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingTop;

	/** additional canvas padding: bottom */
	UPROPERTY(config, EditAnywhere, Category = Display)
	float DebugCanvasPaddingBottom;

	UPROPERTY(config, EditAnywhere, Category = AddOns, EditFixedSize)
	TArray<FGameplayDebuggerCategoryConfig> Categories;

	UPROPERTY(config, EditAnywhere, Category = AddOns, EditFixedSize)
	TArray<FGameplayDebuggerExtensionConfig> Extensions;

	/** updates entry in Categories array and modifies category creation params */
	void UpdateCategoryConfig(const FName CategoryName, int32& SlotIdx, uint8& CategoryState);

	/** updates entry in Categories array and modifies input binding params */
	void UpdateCategoryInputConfig(const FName CategoryName, const FName InputName, FName& KeyName, FGameplayDebuggerInputModifier& KeyModifier);

	/** updates entry in Extensions array and modifies extension creation params */
	void UpdateExtensionConfig(const FName ExtensionName, uint8& UseExtension);

	/** updates entry in Categories array and modifies input binding params */
	void UpdateExtensionInputConfig(const FName ExtensionName, const FName InputName, FName& KeyName, FGameplayDebuggerInputModifier& KeyModifier);

	/** remove all category and extension data from unknown sources (outdated entries) */
	void RemoveUnknownConfigs();

	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:

	/** used for cleanup */
	TArray<FName> KnownCategoryNames;
	TArray<FName> KnownExtensionNames;
	TMultiMap<FName, FName> KnownCategoryInputNames;
	TMultiMap<FName, FName> KnownExtensionInputNames;
};
