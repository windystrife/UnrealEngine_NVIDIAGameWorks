// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Character.h"
#include "ArchVisCharacter.generated.h"

class UInputComponent;

UCLASS(Blueprintable)
class ARCHVISCHARACTER_API AArchVisCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** Ctor */
	AArchVisCharacter(const FObjectInitializer& ObjectInitializer);

	/** Axis name for direct look up/down inputs (e.g. mouse). This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString LookUpAxisName;

	/** Axis name for rate-based look up/down inputs (e.g. joystick). This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString LookUpAtRateAxisName;

	/** Axis name for direct turn left/right inputs (e.g. mouse). This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString TurnAxisName;

	/** Axis name for rate-based turn left/right inputs (e.g. joystick). This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString TurnAtRateAxisName;

	/** Axis name for "move forward/back" control. This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString MoveForwardAxisName;
	
	/** Axis name for "move left/right" control. This should match an Axis Binding in your input settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	FString MoveRightAxisName;

	/** Controls how aggressively mouse motion translates to character rotation in the pitch axis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	float MouseSensitivityScale_Pitch;

	/** Controls how aggressively mouse motion translates to character rotation in the yaw axis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ArchVis Controls")
	float MouseSensitivityScale_Yaw;

protected:
	
	// helper
	class UArchVisCharMovementComponent* GetArchVisCharMoveComponent() const;

	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual FRotator GetViewRotation() const override;

	// input handlers
	virtual void Turn(float Val);
	virtual void TurnAtRate(float Val);
	virtual void LookUp(float Val);
	virtual void LookUpAtRate(float Val);
	virtual void MoveRight(float Val);
	virtual void MoveForward(float Val);

};
