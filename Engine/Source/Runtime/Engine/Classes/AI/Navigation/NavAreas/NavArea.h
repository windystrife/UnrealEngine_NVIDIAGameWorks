// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavArea.generated.h"

/** Class containing definition of a navigation area */
UCLASS(DefaultToInstanced, abstract, Config=Engine, Blueprintable)
class ENGINE_API UNavArea : public UObject
{
	GENERATED_UCLASS_BODY()

	/** travel cost multiplier for path distance */
	UPROPERTY(EditAnywhere, Category=NavArea, config, meta=(ClampMin = "0.0"))
	float DefaultCost;

protected:
	/** entering cost */
	UPROPERTY(EditAnywhere, Category=NavArea, config, meta=(ClampMin = "0.0"))
	float FixedAreaEnteringCost;

public:
	/** area color in navigation view */
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	FColor DrawColor;

	/** restrict area only to specified agents */
	UPROPERTY(EditAnywhere, Category=NavArea, config)
	FNavAgentSelector SupportedAgents;

	// DEPRECATED AGENT CONFIG
#if CPP
	union
	{
		struct
		{
#endif
	UPROPERTY(config)
	uint32 bSupportsAgent0 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent1 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent2 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent3 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent4 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent5 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent6 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent7 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent8 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent9 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent10 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent11 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent12 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent13 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent14 : 1;
	UPROPERTY(config)
	uint32 bSupportsAgent15 : 1;
#if CPP
		};
		uint32 SupportedAgentsBits;
	};
#endif

	virtual void FinishDestroy() override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;

	FORCEINLINE uint16 GetAreaFlags() const { return AreaFlags; }
	FORCEINLINE bool HasFlags(uint16 InFlags) const { return (InFlags & AreaFlags) != 0; }

	FORCEINLINE bool IsSupportingAgent(int32 AgentIndex) const { return SupportedAgents.Contains(AgentIndex); }

	/** called before adding to navigation system */
	virtual void InitializeArea() {};

	/** Get the fixed area entering cost. */
	virtual float GetFixedAreaEnteringCost() { return FixedAreaEnteringCost; }

	/** Retrieved color declared for AreaDefinitionClass */
	static FColor GetColor(UClass* AreaDefinitionClass);

#if WITH_EDITOR
	/** setup agent related properties */
	virtual void UpdateAgentConfig();
#endif
	/** copy properties from other area */
	virtual void CopyFrom(TSubclassOf<UNavArea> AreaClass);

protected:

	/** these flags will be applied to navigation data along with AreaID */
	uint16 AreaFlags;
	
	void RegisterArea();
};
