// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "NavAreaMeta.generated.h"

class AActor;

/** Class containing definition of a navigation area */
UCLASS(Abstract)
class ENGINE_API UNavAreaMeta : public UNavArea
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	Picks an navigation area class that should be used for Actor when
	 *	queried by NavAgent
	 */
	virtual TSubclassOf<UNavArea> PickAreaClass(const AActor* Actor, const FNavAgentProperties& NavAgent) const;

	FORCEINLINE static TSubclassOf<UNavArea> PickAreaClass(TSubclassOf<UNavArea> AreaClass, const AActor* Actor, const FNavAgentProperties& NavAgent)
	{
		if (AreaClass->IsChildOf(UNavAreaMeta::StaticClass()))
		{
			return ((UNavAreaMeta*)AreaClass->GetDefaultObject())->PickAreaClass(Actor, NavAgent);
		}

		return AreaClass;
	}	
	
protected:

	/** Returns index of nav agent */
	int8 GetNavAgentIndex(const FNavAgentProperties& NavAgent) const;
};
