// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRig.h"
#include "GameFramework/Actor.h"
#include "Misc/RuntimeErrors.h"

#define LOCTEXT_NAMESPACE "ControlRig"

const FName UControlRig::AnimationInputMetaName("AnimationInput");
const FName UControlRig::AnimationOutputMetaName("AnimationOutput");

UControlRig::UControlRig()
	: DeltaTime(0.0f)
{
}

UWorld* UControlRig::GetWorld() const
{
	AActor* HostingActor = GetHostingActor();
	return HostingActor ? HostingActor->GetWorld() : nullptr;
}

void UControlRig::Initialize()
{
	OnInitialize();
}

void UControlRig::BindToObject(UObject* InObject)
{

}

void UControlRig::UnbindFromObject()
{

}

bool UControlRig::IsBoundToObject(UObject* InObject) const
{
	return false;
}

UObject* UControlRig::GetBoundObject() const
{
	return nullptr;
}

void UControlRig::PreEvaluate()
{
	
}

void UControlRig::Evaluate()
{
	OnEvaluate();
}

void UControlRig::PostEvaluate()
{

}

UControlRig* UControlRig::EvaluateControlRig(UControlRig* Target)
{
	if (ensureAsRuntimeWarning(Target != nullptr))
	{
		Target->PreEvaluate();
		Target->Evaluate();
		Target->PostEvaluate();
	}
	return Target;
}

UControlRig* UControlRig::EvaluateControlRigWithInputs(UControlRig* Target, FPreEvaluateGatherInputs PreEvaluate)
{
	PreEvaluate.ExecuteIfBound();
	if (ensureAsRuntimeWarning(Target != nullptr))
	{
		Target->PreEvaluate();
		Target->Evaluate();
		Target->PostEvaluate();
	}
	return Target;
}

#if WITH_EDITOR
FText UControlRig::GetCategory() const
{
	return LOCTEXT("DefaultControlRigCategory", "Animation|ControlRigs");
}

FText UControlRig::GetTooltipText() const
{
	return LOCTEXT("DefaultControlRigTooltip", "ControlRig");
}
#endif

void UControlRig::SetDeltaTime(float InDeltaTime)
{
	DeltaTime = InDeltaTime;
}

float UControlRig::GetDeltaTime() const
{
	return DeltaTime;
}

UControlRig* UControlRig::GetOrAllocateSubControlRig(TSubclassOf<UControlRig> ControlRigClass, int32 AllocationIndex)
{
	if (ensureAsRuntimeWarning(ControlRigClass != nullptr))
	{
		if (ensureAsRuntimeWarning(AllocationIndex >= 0))
		{
			// reallocate the sub ControlRig reserve to accommodate this index
			if (!SubControlRigs.IsValidIndex(AllocationIndex))
			{
				SubControlRigs.SetNum(FMath::Max(SubControlRigs.Num(), AllocationIndex + 1), false);
			}

			if (SubControlRigs[AllocationIndex] == nullptr)
			{
				SubControlRigs[AllocationIndex] = NewObject<UControlRig>(this, ControlRigClass.Get(), "SubControlRig");
			}

			return SubControlRigs[AllocationIndex];
		}
	}

	return nullptr;
}

AActor* UControlRig::GetHostingActor() const
{
	UObject* Outer = GetOuter();
	while (Outer && !Outer->IsA<AActor>())
	{
		Outer = Outer->GetOuter();
	}

	return Cast<AActor>(Outer);
}

#undef LOCTEXT_NAMESPACE