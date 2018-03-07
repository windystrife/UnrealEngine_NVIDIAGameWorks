// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Pawn.h"
#include "WheeledVehicle.generated.h"

class FDebugDisplayInfo;

/**
 * WheeledVehicle is the base wheeled vehicle pawn actor.
 * By default it uses UWheeledVehicleMovementComponent4W for its simulation, but this can be overridden by inheriting from the class and modifying its constructor like so:
 * Super(ObjectInitializer.SetDefautSubobjectClass<UMyMovement>(VehicleMovementComponentName))
 * Where UMyMovement is the new movement type that inherits from UWheeledVehicleMovementComponent
 * 
 * @see https://docs.unrealengine.com/latest/INT/Engine/Physics/Vehicles/VehicleUserGuide/
 * @see UWheeledVehicleMovementComponent4W
 */

UCLASS(abstract, config=Game, BlueprintType)
class PHYSXVEHICLES_API AWheeledVehicle : public APawn
{
	GENERATED_UCLASS_BODY()

private:
	/**  The main skeletal mesh associated with this Vehicle */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* Mesh;

	/** vehicle simulation component */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWheeledVehicleMovementComponent* VehicleMovement;
public:

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName VehicleMeshComponentName;

	/** Name of the VehicleMovement. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName VehicleMovementComponentName;

	/** Util to get the wheeled vehicle movement component */
	class UWheeledVehicleMovementComponent* GetVehicleMovementComponent() const;

	//~ Begin AActor Interface
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	//~ End Actor Interface

	/** Returns Mesh subobject **/
	class USkeletalMeshComponent* GetMesh() const { return Mesh; }
	/** Returns VehicleMovement subobject **/
	class UWheeledVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }
};
