// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/Interface.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimStateMachineTypes.h"

#include "AnimClassInterface.generated.h"

class USkeleton;

UINTERFACE()
class ENGINE_API UAnimClassInterface : public UInterface
{
	GENERATED_BODY()
};

class ENGINE_API IAnimClassInterface
{
	GENERATED_BODY()
public:
	virtual const TArray<FBakedAnimationStateMachine>& GetBakedStateMachines() const = 0;
	virtual const TArray<FAnimNotifyEvent>& GetAnimNotifies() const = 0;
	virtual const TArray<UStructProperty*>& GetAnimNodeProperties() const = 0;
	virtual const TArray<FName>& GetSyncGroupNames() const = 0;
	virtual const TArray<int32>& GetOrderedSavedPoseNodeIndices() const = 0;

	virtual USkeleton* GetTargetSkeleton() const = 0;
	virtual int32 GetRootAnimNodeIndex() const = 0;
	virtual UStructProperty* GetRootAnimNodeProperty() const = 0;
	virtual int32 GetSyncGroupIndex(FName SyncGroupName) const = 0;

	static IAnimClassInterface* GetFromClass(UClass* InClass)
	{
		if (auto AnimClassInterface = Cast<IAnimClassInterface>(InClass))
		{
			return AnimClassInterface;
		}
		if (auto DynamicClass = Cast<UDynamicClass>(InClass))
		{
			DynamicClass->GetDefaultObject(true);
			return CastChecked<IAnimClassInterface>(DynamicClass->AnimClassImplementation, ECastCheckedType::NullAllowed);
		}
		return nullptr;
	}

	static UClass* GetActualAnimClass(IAnimClassInterface* AnimClassInterface)
	{
		if (UClass* ActualAnimClass = Cast<UClass>(AnimClassInterface))
		{
			return ActualAnimClass;
		}
		if (UObject* AsObject = Cast<UObject>(AnimClassInterface))
		{
			return Cast<UClass>(AsObject->GetOuter());
		}
		return nullptr;
	}
};
