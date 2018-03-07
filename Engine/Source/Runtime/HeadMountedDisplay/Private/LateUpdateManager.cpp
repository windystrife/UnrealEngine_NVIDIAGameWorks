// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LateUpdateManager.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "PrimitiveSceneInfo.h"

FLateUpdateManager::FLateUpdateManager() 
	: LateUpdateGameWriteIndex(0)
	, LateUpdateRenderReadIndex(0)
{
}

void FLateUpdateManager::Setup(const FTransform& ParentToWorld, USceneComponent* Component)
{
	check(IsInGameThread());
	LateUpdateParentToWorld[LateUpdateGameWriteIndex] = ParentToWorld;
	LateUpdatePrimitives[LateUpdateGameWriteIndex].Reset();
	GatherLateUpdatePrimitives(Component);
	LateUpdateGameWriteIndex = (LateUpdateGameWriteIndex + 1) % 2;
}

void FLateUpdateManager::Apply_RenderThread(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform)
{
	check(IsInRenderingThread());
	if (!LateUpdatePrimitives[LateUpdateRenderReadIndex].Num())
	{
		return;
	}

	const FTransform OldCameraTransform = OldRelativeTransform * LateUpdateParentToWorld[LateUpdateRenderReadIndex];
	const FTransform NewCameraTransform = NewRelativeTransform * LateUpdateParentToWorld[LateUpdateRenderReadIndex];
	const FMatrix LateUpdateTransform = (OldCameraTransform.Inverse() * NewCameraTransform).ToMatrixWithScale();

	// Apply delta to the affected scene proxies
	for (auto PrimitiveInfo : LateUpdatePrimitives[LateUpdateRenderReadIndex])
	{
		FPrimitiveSceneInfo* RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(*PrimitiveInfo.IndexAddress);
		FPrimitiveSceneInfo* CachedSceneInfo = PrimitiveInfo.SceneInfo;
		// If the retrieved scene info is different than our cached scene info then the primitive was removed from the scene
		if (CachedSceneInfo == RetrievedSceneInfo && CachedSceneInfo->Proxy)
		{
			CachedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
		}
	}
	LateUpdatePrimitives[LateUpdateRenderReadIndex].Reset();
	LateUpdateRenderReadIndex = (LateUpdateRenderReadIndex + 1) % 2;
}

void FLateUpdateManager::CacheSceneInfo(USceneComponent* Component)
{
	// If a scene proxy is present, cache it
	UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveComponent->SceneProxy->GetPrimitiveSceneInfo();
		if (PrimitiveSceneInfo)
		{
			LateUpdatePrimitiveInfo PrimitiveInfo;
			PrimitiveInfo.IndexAddress = PrimitiveSceneInfo->GetIndexAddress();
			PrimitiveInfo.SceneInfo = PrimitiveSceneInfo;
			LateUpdatePrimitives[LateUpdateGameWriteIndex].Add(PrimitiveInfo);
		}
	}
}

void FLateUpdateManager::GatherLateUpdatePrimitives(USceneComponent* ParentComponent)
{
	CacheSceneInfo(ParentComponent);

	TArray<USceneComponent*> Components;
	ParentComponent->GetChildrenComponents(true, Components);
	for(USceneComponent* Component : Components)
	{
		check(Component != nullptr);
		CacheSceneInfo(Component);
	}
}