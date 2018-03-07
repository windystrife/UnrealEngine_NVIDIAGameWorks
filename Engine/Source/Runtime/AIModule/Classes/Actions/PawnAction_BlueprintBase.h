// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Actions/PawnAction.h"
#include "PawnAction_BlueprintBase.generated.h"

class APawn;

UCLASS(abstract, Blueprintable)
class AIMODULE_API UPawnAction_BlueprintBase : public UPawnAction
{
	GENERATED_UCLASS_BODY()

public:

	//----------------------------------------------------------------------//
	// Blueprint interface
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	void ActionStart(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	void ActionTick(APawn* ControlledPawn, float DeltaSeconds);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	void ActionPause(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	void ActionResume(APawn* ControlledPawn);
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|PawnActions")
	void ActionFinished(APawn* ControlledPawn, EPawnActionResult::Type WithResult);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual bool Start() override;
	virtual bool Pause(const UPawnAction* PausedBy) override;
	virtual bool Resume() override;
	virtual void OnFinished(EPawnActionResult::Type WithResult) override;
};
