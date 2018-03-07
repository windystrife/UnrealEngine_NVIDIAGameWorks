// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Movement component meant for use with Pawns.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/NavMovementComponent.h"
#include "PawnMovementComponent.generated.h"

class APawn;

/** 
 * PawnMovementComponent can be used to update movement for an associated Pawn.
 * It also provides ways to accumulate and read directional input in a generic way (with AddInputVector(), ConsumeInputVector(), etc).
 * @see APawn
 */
UCLASS(abstract)
class ENGINE_API UPawnMovementComponent : public UNavMovementComponent
{
	GENERATED_BODY()

public:

	/** Overridden to only allow registration with components owned by a Pawn. */
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

	/**
	 * Adds the given vector to the accumulated input in world space. Input vectors are usually between 0 and 1 in magnitude. 
	 * They are accumulated during a frame then applied as acceleration during the movement update.
	 *
	 * @param WorldDirection	Direction in world space to apply input
	 * @param ScaleValue		Scale to apply to input. This can be used for analog input, ie a value of 0.5 applies half the normal value.
	 * @param bForce			If true always add the input, ignoring the result of IsMoveInputIgnored().
	 * @see APawn::AddMovementInput()
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	virtual void AddInputVector(FVector WorldVector, bool bForce = false);

	/**
	 * Return the pending input vector in world space. This is the most up-to-date value of the input vector, pending ConsumeMovementInputVector() which clears it.
	 * PawnMovementComponents implementing movement usually want to use either this or ConsumeInputVector() as these functions represent the most recent state of input.
	 * @return The pending input vector in world space.
	 * @see AddInputVector(), ConsumeInputVector(), GetLastInputVector()
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement", meta=(Keywords="GetInput"))
	FVector GetPendingInputVector() const;

	/**
	* Return the last input vector in world space that was processed by ConsumeInputVector(), which is usually done by the Pawn or PawnMovementComponent.
	* Any user that needs to know about the input that last affected movement should use this function.
	* @return The last input vector in world space that was processed by ConsumeInputVector().
	* @see AddInputVector(), ConsumeInputVector(), GetPendingInputVector()
	*/
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement", meta=(Keywords="GetInput"))
	FVector GetLastInputVector() const;

	/* Returns the pending input vector and resets it to zero.
	 * This should be used during a movement update (by the Pawn or PawnMovementComponent) to prevent accumulation of control input between frames.
	 * Copies the pending input vector to the saved input vector (GetLastMovementInputVector()).
	 * @return The pending input vector.
	 */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	virtual FVector ConsumeInputVector();

	/** Helper to see if move input is ignored. If there is no Pawn or UpdatedComponent, returns true, otherwise defers to the Pawn's implementation of IsMoveInputIgnored(). */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	virtual bool IsMoveInputIgnored() const;

	/** Return the Pawn that owns UpdatedComponent. */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	class APawn* GetPawnOwner() const;

	/** Notify of collision in case we want to react, such as waking up avoidance or pathing code. */
	virtual void NotifyBumpedPawn(APawn* BumpedPawn) {}

	// UNavMovementComponent override for input operations
	virtual void RequestPathMove(const FVector& MoveInput) override;

protected:

	/** Pawn that owns this component. */
	UPROPERTY(Transient, DuplicateTransient)
	class APawn* PawnOwner;

public:

	virtual void Serialize(FArchive& Ar) override;

	// DEPRECATED FUNCTIONS

	/** (Deprecated) Return the input vector in world space. */
	DEPRECATED(4.5, "GetInputVector() has been deprecated, use either GetPendingInputVector() or GetLastInputVector().")
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement", meta=(DeprecatedFunction, DisplayName="GetInputVector", DeprecationMessage="GetInputVector has been deprecated, use either GetPendingInputVector or GetLastInputVector"))
	FVector K2_GetInputVector() const;
};
