// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigComponent.h"
#include "ControlRig.h"
#include "ComponentInstanceDataCache.h"

#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"

#if WITH_EDITOR
#include "BlueprintEditorUtils.h"
#endif
////////////////////////////////////////////////////////////////////////////////////////

/** Used to store animation ControlRig data during recompile of BP */
class FControlRigComponentInstanceData : public FActorComponentInstanceData
{
public:
	FControlRigComponentInstanceData(const UControlRigComponent* SourceComponent)
		: FActorComponentInstanceData(SourceComponent)
		, AnimControlRig(SourceComponent->ControlRig)
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FActorComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		UControlRigComponent* NewComponent = CastChecked<UControlRigComponent>(Component);

		UControlRig* NewControlRig = NewComponent->ControlRig;
		if (NewControlRig && AnimControlRig)
		{
			// it will just copy same property if not same class
			TArray<uint8> SavedPropertyBuffer;
			FObjectWriter Writer(AnimControlRig, SavedPropertyBuffer);
			FObjectReader Reader(NewComponent->ControlRig, SavedPropertyBuffer);
		}
	}

	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) override
	{
		FActorComponentInstanceData::FindAndReplaceInstances(OldToNewInstanceMap);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FActorComponentInstanceData::AddReferencedObjects(Collector);
		if (AnimControlRig)
		{
			Collector.AddReferencedObject(AnimControlRig);
		}
	}

	bool ContainsData() const
	{
		return (AnimControlRig != nullptr);
	}

	// stored object
	UControlRig* AnimControlRig;
};

////////////////////////////////////////////////////////////////////////////////////////
UControlRigComponent::UControlRigComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bNeedsInitialization(false)
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

#if WITH_EDITOR
void UControlRigComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigComponent, ControlRig))
	{
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			FBlueprintEditorUtils::ReconstructAllNodes(Blueprint);
		}
	}
}
#endif

void UControlRigComponent::OnRegister()
{
	Super::OnRegister();
	bNeedsInitialization = true;

	RegisterTickDependencies();
}

void UControlRigComponent::OnUnregister()
{
	Super::OnUnregister();

	UnRegisterTickDependencies();
}

void UControlRigComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	RegisterTickDependencies();

	// @TODO: Add task to perform evaluation rather than performing it here.
	// @TODO: Double buffer ControlRig?
	
	if (ControlRig)
	{
		if (bNeedsInitialization)
		{
			OnPreInitialize();
			ControlRig->Initialize();
			OnPostInitialize();
			bNeedsInitialization = false;
		}

		ControlRig->SetDeltaTime(DeltaTime);
		OnPreEvaluate();
		ControlRig->PreEvaluate();

		// @TODO: If we were to multi-thread execution, Evaluate() should probably be the call that gets made on 
		// a worker thread and the order of PreEvaluate/Evaluate/PostEvaluate should be preserved
		ControlRig->Evaluate();

		ControlRig->PostEvaluate();
		OnPostEvaluate();
	}
}

UControlRig* UControlRigComponent::BP_GetControlRig() const
{
	return ControlRig;
}

void UControlRigComponent::OnPreInitialize_Implementation()
{
	OnPreInitializeDelegate.Broadcast(this);
}

void UControlRigComponent::OnPostInitialize_Implementation()
{
	OnPostInitializeDelegate.Broadcast(this);
}

void UControlRigComponent::OnPreEvaluate_Implementation()
{
	OnPreEvaluateDelegate.Broadcast(this);
}

void UControlRigComponent::OnPostEvaluate_Implementation()
{
	OnPostEvaluateDelegate.Broadcast(this);
}

void UControlRigComponent::RegisterTickDependencies()
{
	if (ControlRig)
	{
		TArray<FTickPrerequisite, TInlineAllocator<1>> TickPrerequisites;
		ControlRig->GetTickDependencies(TickPrerequisites);
		for (const FTickPrerequisite& TickPrerequisite : TickPrerequisites)
		{
			PrimaryComponentTick.AddPrerequisite(TickPrerequisite.PrerequisiteObject.Get(), *TickPrerequisite.PrerequisiteTickFunction);
		}
	}
}

void UControlRigComponent::UnRegisterTickDependencies()
{
	if (ControlRig)
	{
		TArray<FTickPrerequisite, TInlineAllocator<1>> TickPrerequisites;
		ControlRig->GetTickDependencies(TickPrerequisites);
		for (const FTickPrerequisite& TickPrerequisite : TickPrerequisites)
		{
			if (TickPrerequisite.PrerequisiteObject.IsValid() && TickPrerequisite.PrerequisiteTickFunction)
			{
				PrimaryComponentTick.RemovePrerequisite(TickPrerequisite.PrerequisiteObject.Get(), *TickPrerequisite.PrerequisiteTickFunction);
			}
		}
	}
}
FActorComponentInstanceData* UControlRigComponent::GetComponentInstanceData() const
{
	FControlRigComponentInstanceData* InstanceData = new FControlRigComponentInstanceData(this);

	if (!InstanceData->ContainsData())
	{
		delete InstanceData;
		InstanceData = nullptr;
	}

	return InstanceData;
}