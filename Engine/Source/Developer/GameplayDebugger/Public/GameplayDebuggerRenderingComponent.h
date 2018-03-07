// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "DebugRenderSceneProxy.h"
#include "GameplayDebuggerRenderingComponent.generated.h"

class FGameplayDebuggerCompositeSceneProxy;

class FGameplayDebuggerDebugDrawDelegateHelper : public FDebugDrawDelegateHelper
{
	typedef FDebugDrawDelegateHelper Super;

public:
	~FGameplayDebuggerDebugDrawDelegateHelper()
	{
		Reset();
	}

	void Reset();

	void AddDelegateHelper(FDebugDrawDelegateHelper* InDebugDrawDelegateHelper);

	virtual void RegisterDebugDrawDelgate() override;
	virtual void UnregisterDebugDrawDelgate() override;

private:
	TArray<FDebugDrawDelegateHelper*> DebugDrawDelegateHelpers;
};



UCLASS(NotBlueprintable, NotBlueprintType, noteditinlinenew, hidedropdown, Transient)
class UGameplayDebuggerRenderingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;

	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;

	FGameplayDebuggerDebugDrawDelegateHelper GameplayDebuggerDebugDrawDelegateHelper;
};
