// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"
#include "AI/Navigation/NavMeshRenderingComponent.h"

class APlayerController;

class FGameplayDebuggerCategory_Navmesh : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Navmesh();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy(const UPrimitiveComponent* InComponent, FDebugDrawDelegateHelper*& OutDelegateHelper) override;
	virtual void OnDataPackReplicated(int32 DataPackId) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	FNavMeshSceneProxyData NavmeshRenderData;
};

#endif // WITH_GAMEPLAY_DEBUGGER
