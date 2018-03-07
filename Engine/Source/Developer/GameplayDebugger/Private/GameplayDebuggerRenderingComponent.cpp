// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerRenderingComponent.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayDebuggerCategory.h"

//////////////////////////////////////////////////////////////////////////
// FGameplayDebuggerCompositeSceneProxy

class FGameplayDebuggerCompositeSceneProxy : public FDebugRenderSceneProxy
{
	friend class FGameplayDebuggerDebugDrawDelegateHelper;
public:
	FGameplayDebuggerCompositeSceneProxy(const UPrimitiveComponent* InComponent) : FDebugRenderSceneProxy(InComponent) { }

	virtual ~FGameplayDebuggerCompositeSceneProxy()
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			delete ChildProxies[Idx];
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->DrawStaticElements(PDI);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			ChildProxies[Idx]->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Result |= ChildProxies[Idx]->GetViewRelevance(View);
		}
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize(void) const
	{
		uint32 Size = ChildProxies.GetAllocatedSize();
		for (int32 Idx = 0; Idx < ChildProxies.Num(); Idx++)
		{
			Size += ChildProxies[Idx]->GetMemoryFootprint();
		}

		return Size;
	}

	void AddChild(FDebugRenderSceneProxy* NewChild)
	{
		ChildProxies.AddUnique(NewChild);
	}

	void AddRange(TArray<FDebugRenderSceneProxy*> Children)
	{
		ChildProxies.Append(Children);
	}

protected:
	TArray<FDebugRenderSceneProxy*> ChildProxies;
};

void FGameplayDebuggerDebugDrawDelegateHelper::RegisterDebugDrawDelgate()
{
	ensureMsgf(State != RegisteredState, TEXT("RegisterDebugDrawDelgate is already Registered!"));
	if (State == InitializedState)
	{
		for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
		{
			DebugDrawDelegateHelpers[Idx]->RegisterDebugDrawDelgate();
		}
		State = RegisteredState;
	}
}

void FGameplayDebuggerDebugDrawDelegateHelper::UnregisterDebugDrawDelgate()
{
	ensureMsgf(State != InitializedState, TEXT("UnegisterDebugDrawDelgate is in an invalid State: %i !"), State);
	if (State == RegisteredState)
	{
		for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
		{
			DebugDrawDelegateHelpers[Idx]->UnregisterDebugDrawDelgate();
		}
		State = InitializedState;
	}
}

void FGameplayDebuggerDebugDrawDelegateHelper::Reset()
{
	for (int32 Idx = 0; Idx < DebugDrawDelegateHelpers.Num(); Idx++)
	{
		delete DebugDrawDelegateHelpers[Idx];
	}
	DebugDrawDelegateHelpers.Reset();
}

void FGameplayDebuggerDebugDrawDelegateHelper::AddDelegateHelper(FDebugDrawDelegateHelper* InDebugDrawDelegateHelper)
{
	check(InDebugDrawDelegateHelper);
	DebugDrawDelegateHelpers.Add(InDebugDrawDelegateHelper);
}

//////////////////////////////////////////////////////////////////////////
// UGameplayDebuggerRenderingComponent

UGameplayDebuggerRenderingComponent::UGameplayDebuggerRenderingComponent(const FObjectInitializer& ObjInitializer) : Super(ObjInitializer)
{
}

FPrimitiveSceneProxy* UGameplayDebuggerRenderingComponent::CreateSceneProxy()
{
	GameplayDebuggerDebugDrawDelegateHelper.Reset();

	FGameplayDebuggerCompositeSceneProxy* CompositeProxy = nullptr;

	AGameplayDebuggerCategoryReplicator* OwnerReplicator = Cast<AGameplayDebuggerCategoryReplicator>(GetOwner());
	if (OwnerReplicator && OwnerReplicator->IsEnabled())
	{
		TArray<FDebugRenderSceneProxy*> SceneProxies;
		for (int32 Idx = 0; Idx < OwnerReplicator->GetNumCategories(); Idx++)
		{
			TSharedRef<FGameplayDebuggerCategory> Category = OwnerReplicator->GetCategory(Idx);
			if (Category->IsCategoryEnabled())
			{
				FDebugDrawDelegateHelper* DebugDrawDelegateHelper = nullptr;
				FDebugRenderSceneProxy* CategorySceneProxy = Category->CreateDebugSceneProxy(this, DebugDrawDelegateHelper);
				if (CategorySceneProxy)
				{
					SceneProxies.Add(CategorySceneProxy);
				}

				if (DebugDrawDelegateHelper)
				{
					GameplayDebuggerDebugDrawDelegateHelper.AddDelegateHelper(DebugDrawDelegateHelper);
				}
			}
		}

		if (SceneProxies.Num())
		{
			CompositeProxy = new FGameplayDebuggerCompositeSceneProxy(this);
			CompositeProxy->AddRange(SceneProxies);
		}
	}

	if (CompositeProxy)
	{
		GameplayDebuggerDebugDrawDelegateHelper.InitDelegateHelper(CompositeProxy);
		GameplayDebuggerDebugDrawDelegateHelper.ReregisterDebugDrawDelgate();
	}
	return CompositeProxy;
}

FBoxSphereBounds UGameplayDebuggerRenderingComponent::CalcBounds(const FTransform &LocalToWorld) const
{
	return FBoxSphereBounds(FBox::BuildAABB(FVector::ZeroVector, FVector(1000000.0f, 1000000.0f, 1000000.0f)));
}

void UGameplayDebuggerRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	GameplayDebuggerDebugDrawDelegateHelper.RegisterDebugDrawDelgate();
}

void UGameplayDebuggerRenderingComponent::DestroyRenderState_Concurrent()
{
	GameplayDebuggerDebugDrawDelegateHelper.UnregisterDebugDrawDelgate();

	Super::DestroyRenderState_Concurrent();
}
