// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/TriggerBase.h"
#include "TriggerBox.generated.h"

/** A box shaped trigger, used to generate overlap events in the level */
UCLASS()
class ENGINE_API ATriggerBox : public ATriggerBase
{
	GENERATED_UCLASS_BODY()


#if WITH_EDITOR
	//~ Begin AActor Interface.
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	//~ End AActor Interface.
#endif
};



