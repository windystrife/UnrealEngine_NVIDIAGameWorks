// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "PhysxUserData.h"
#include "Vehicles/TireType.h"
#include "PhysicsEngine/PhysicsSettingsEnums.h"
#include "PhysicalMaterial.generated.h"

struct FPropertyChangedEvent;

namespace physx
{
	class PxMaterial;
}

/** DEPRECATED Pairs desired tire friction scale with tire type */
USTRUCT()
struct FTireFrictionScalePair
{
	GENERATED_USTRUCT_BODY()

	/** Tire type */
	UPROPERTY()
	class UTireType*				TireType;

	/** Friction scale for this type of tire */
	UPROPERTY()
	float							FrictionScale;

	FTireFrictionScalePair()
		: TireType(NULL)
		, FrictionScale(1.0f)
	{
	}
};

/**
 * Physical materials are used to define the response of a physical object when interacting dynamically with the world.
 */
UCLASS(BlueprintType, Blueprintable, CollapseCategories, HideCategories = Object)
class ENGINE_API UPhysicalMaterial : public UObject
{
	GENERATED_UCLASS_BODY()

	//
	// Surface properties.
	//
	
	/** Friction value of surface, controls how easily things can slide on this surface (0 is frictionless, higher values increase the amount of friction) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PhysicalMaterial, meta=(ClampMin=0))
	float Friction;

	/** Friction combine mode, controls how friction is computed for multiple materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PhysicalMaterial, meta = (editcondition = "bOverrideFrictionCombineMode"))
	TEnumAsByte<EFrictionCombineMode::Type> FrictionCombineMode;

	/** If set we will use the FrictionCombineMode of this material, instead of the FrictionCombineMode found in the project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicalMaterial)
	bool bOverrideFrictionCombineMode;

	/** Restitution or 'bounciness' of this surface, between 0 (no bounce) and 1 (outgoing velocity is same as incoming). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PhysicalMaterial,  meta=(ClampMin=0, ClampMax=1))
	float Restitution;

	/** Restitution combine mode, controls how restitution is computed for multiple materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PhysicalMaterial, meta = (editcondition = "bOverrideRestitutionCombineMode"))
	TEnumAsByte<EFrictionCombineMode::Type> RestitutionCombineMode;

	/** If set we will use the RestitutionCombineMode of this material, instead of the RestitutionCombineMode found in the project settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicalMaterial)
	bool bOverrideRestitutionCombineMode;

	//
	// Object properties.
	//
	
	/** Used with the shape of the object to calculate its mass properties. The higher the number, the heavier the object. g per cubic cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PhysicalMaterial, meta=(ClampMin=0))
	float Density;

	/** 
	 *	Used to adjust the way that mass increases as objects get larger. This is applied to the mass as calculated based on a 'solid' object. 
	 *	In actuality, larger objects do not tend to be solid, and become more like 'shells' (e.g. a car is not a solid piece of metal).
	 *	Values are clamped to 1 or less.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Advanced, meta=(ClampMin=0.1, ClampMax=1))
	float RaiseMassToPower;

	/** How much to scale the damage threshold by on any destructible we are applied to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Destruction)
	float DestructibleDamageThresholdScale;

	UPROPERTY(/*deprecated*/)
	class UDEPRECATED_PhysicalMaterialPropertyBase* PhysicalMaterialProperty;

	/**
	 * To edit surface type for your project, use ProjectSettings/Physics/PhysicalSurface section
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = PhysicalProperties)
	TEnumAsByte<EPhysicalSurface> SurfaceType;

	/** DEPRECATED - Overall tire friction scalar for every type of tire. This value is multiplied against our parents' values. */
	UPROPERTY(VisibleAnywhere, Category = Deprecated)
	float TireFrictionScale;

	/** DEPRECATED - Tire friction scales for specific types of tires. These values are multiplied against our parents' values. */
	UPROPERTY(VisibleAnywhere, Category = Deprecated)
	TArray<FTireFrictionScalePair> TireFrictionScales;

public:
#if WITH_PHYSX
	/** Internal pointer to PhysX material object */
	physx::PxMaterial* PMaterial;

	FPhysxUserData PhysxUserData;
#endif

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	static void RebuildPhysicalMaterials();
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual void FinishDestroy() override;
	//~ End UObject Interface

#if WITH_PHYSX
	physx::PxMaterial* GetPhysXMaterial();
#endif // WITH_PHYSX

	/** Update the PhysX material from this objects properties */
	void UpdatePhysXMaterial();

	/** Determine Material Type from input PhysicalMaterial **/
	static EPhysicalSurface DetermineSurfaceType(UPhysicalMaterial const* PhysicalMaterial);
};



