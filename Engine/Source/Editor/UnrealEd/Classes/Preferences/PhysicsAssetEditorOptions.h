// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsAssetEditorOptions.generated.h"

UENUM()
enum class EPhysicsAssetEditorRenderMode : uint8
{
	Solid,
	Wireframe,
	None
};

UENUM()
enum class EPhysicsAssetEditorConstraintViewMode : uint8
{
	None,
	AllPositions,
	AllLimits
};

UCLASS(hidecategories=Object, config=EditorPerProjectUserSettings)
class UNREALED_API UPhysicsAssetEditorOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Lets you manually control the physics/animation */
	UPROPERTY(EditAnywhere, transient, Category=Anim, meta=(UIMin=0, UIMax=1, ClampMin=0, ClampMax=1))
	float PhysicsBlend;

	/** Lets you manually control the physics/animation */
	UPROPERTY(EditAnywhere, transient, Category = Anim)
	bool bUpdateJointsFromAnimation;

	/** Determines whether simulation of root body updates component transform */
	UPROPERTY(EditAnywhere, transient, Category = Anim)
	TEnumAsByte<EPhysicsTransformUpdateMode::Type> PhysicsUpdateMode;

	/** Time between poking ragdoll and starting to blend back. */
	UPROPERTY(EditAnywhere, config, Category=Anim)
	float PokePauseTime;

	/** Time taken to blend from physics to animation. */
	UPROPERTY(EditAnywhere, config, Category=Anim)
	float PokeBlendTime;
	
	/** Scale factor for the gravity used in the simulation */
	UPROPERTY(EditAnywhere, config, Category=Simulation, meta=(UIMin=0, UIMax=100, ClampMin=-10000, ClampMax=10000))
	float GravScale;

	/** Max FPS for simulation in PhysicsAssetEditor. This is helpful for targeting the same FPS as your game. -1 means disabled*/
	UPROPERTY(EditAnywhere, config, Category = Simulation)
	int32 MaxFPS;

	/** Linear damping of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleLinearDamping;

	/** Linear stiffness of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleLinearStiffness;

	/** Angular damping of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleAngularDamping;

	/** Angular stiffness of mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float HandleAngularStiffness;

	/** How quickly we interpolate the physics target transform for mouse spring forces */
	UPROPERTY(EditAnywhere, config, Category=MouseSpring)
	float InterpolationSpeed;

	/** Strength of the impulse used when poking with left mouse button */
	UPROPERTY(EditAnywhere, config, Category=Poking)
	float PokeStrength;

	/** Whether to draw constraints as points */
	UPROPERTY(config)
	uint32 bShowConstraintsAsPoints:1;

	/** Controls how large constraints are drawn in Physics Asset Editor */
	UPROPERTY(config)
	float ConstraintDrawSize;

	/** View mode for meshes in edit mode */
	UPROPERTY(config)
	EPhysicsAssetEditorRenderMode MeshViewMode;

	/** View mode for collision in edit mode */
	UPROPERTY(config)
	EPhysicsAssetEditorRenderMode CollisionViewMode;

	/** View mode for constraints in edit mode */
	UPROPERTY(config)
	EPhysicsAssetEditorConstraintViewMode ConstraintViewMode;

	/** View mode for meshes in simulation mode */
	UPROPERTY(config)
	EPhysicsAssetEditorRenderMode SimulationMeshViewMode;

	/** View mode for collision in simulation mode */
	UPROPERTY(config)
	EPhysicsAssetEditorRenderMode SimulationCollisionViewMode;

	/** View mode for constraints in simulation mode */
	UPROPERTY(config)
	EPhysicsAssetEditorConstraintViewMode SimulationConstraintViewMode;

	/** Opacity of 'solid' rendering */
	UPROPERTY(config)
	float CollisionOpacity;

	/** When set, turns opacity of solid rendering for unselected bodies to zero */
	UPROPERTY(config)
	bool bSolidRenderingForSelectedOnly;
};
