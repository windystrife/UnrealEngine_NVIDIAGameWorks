// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/Navigation/NavAreas/NavAreaMeta.h"
#include "NavAreaMeta_SwitchByAgent.generated.h"

class AActor;

/** Class containing definition of a navigation area */
UCLASS(Abstract)
class ENGINE_API UNavAreaMeta_SwitchByAgent : public UNavAreaMeta
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent0Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent1Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent2Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent3Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent4Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent5Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent6Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent7Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent8Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent9Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent10Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent11Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent12Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent13Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent14Area;

	UPROPERTY(EditAnywhere, Category=AgentTypes)
	TSubclassOf<UNavArea> Agent15Area;

	virtual TSubclassOf<UNavArea> PickAreaClass(const AActor* Actor, const FNavAgentProperties& NavAgent) const override;

#if WITH_EDITOR
	/** setup AgentXArea properties */
	virtual void UpdateAgentConfig() override;
#endif
};
