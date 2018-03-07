// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_CurveSource.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_CurveSource::FAnimNode_CurveSource()
	: SourceBinding(ICurveSourceInterface::DefaultBinding)
	, Alpha(1.0f)
{
}

void FAnimNode_CurveSource::PreUpdate(const UAnimInstance* AnimInstance)
{
	// re-bind to our named curve source in pre-update
	// we do this here to allow re-binding of the source without reinitializing the whole
	// anim graph. If the source goes away (e.g. if an audio component is destroyed) or the
	// binding changes then we can re-bind to a new object
	if (CurveSource.GetObject() == nullptr || Cast<ICurveSourceInterface>(CurveSource.GetObject())->Execute_GetBindingName(CurveSource.GetObject()) != SourceBinding)
	{
		ICurveSourceInterface* PotentialCurveSource = nullptr;

		auto IsSpecifiedCurveSource = [&PotentialCurveSource](UObject* InObject, const FName& InSourceBinding, TScriptInterface<ICurveSourceInterface>& InOutCurveSource)
		{
			PotentialCurveSource = Cast<ICurveSourceInterface>(InObject);
			if (PotentialCurveSource && PotentialCurveSource->Execute_GetBindingName(InObject) == InSourceBinding)
			{
				InOutCurveSource.SetObject(InObject);
				InOutCurveSource.SetInterface(PotentialCurveSource);
				return true;
			}

			return false;
		};

		AActor* Actor = AnimInstance->GetOwningActor();
		if (Actor)
		{
			// check if our actor implements our interface
			if (IsSpecifiedCurveSource(Actor, SourceBinding, CurveSource))
			{
				return;
			}

			for (TFieldIterator<UObjectProperty> PropertyIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				UObjectProperty* ObjProp = *PropertyIt;
				UActorComponent* ActorComponent = Cast<UActorComponent>(ObjProp->GetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(Actor)));
				if (IsSpecifiedCurveSource(ActorComponent, SourceBinding, CurveSource))
				{
					return;
				}
			}

			const TSet<UActorComponent*>& ActorOwnedComponents = Actor->GetComponents();
			for (UActorComponent* OwnedComponent : ActorOwnedComponents)
			{
				if (IsSpecifiedCurveSource(OwnedComponent, SourceBinding, CurveSource))
				{
					return;
				}
			}
		}
	}
}

class FExternalCurveScratchArea : public TThreadSingleton<FExternalCurveScratchArea>
{
public:
	TArray<FNamedCurveValue> NamedCurveValues;
};

void FAnimNode_CurveSource::Evaluate_AnyThread(FPoseContext& Output)
{
	SourcePose.Evaluate(Output);

	if (CurveSource.GetInterface() != nullptr)
	{
		TArray<FNamedCurveValue>& NamedCurveValues = FExternalCurveScratchArea::Get().NamedCurveValues;
		NamedCurveValues.Reset();
		CurveSource->Execute_GetCurves(CurveSource.GetObject(), NamedCurveValues);

		USkeleton* Skeleton = Output.AnimInstanceProxy->GetSkeleton();

		for (FNamedCurveValue NamedValue : NamedCurveValues)
		{
			SmartName::UID_Type NameUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, NamedValue.Name);
			if (NameUID != SmartName::MaxUID)
			{
				const float CurrentValue = Output.Curve.Get(NameUID);
				const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
				Output.Curve.Set(NameUID, FMath::Lerp(CurrentValue, NamedValue.Value, ClampedAlpha));
			}
		}
	}
}

void FAnimNode_CurveSource::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// Evaluate any BP logic plugged into this node
	EvaluateGraphExposedInputs.Execute(Context);
}
