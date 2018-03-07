#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "FlowMaterial.generated.h"

USTRUCT(BlueprintType)
struct FFlowMaterialPerComponent
{
	GENERATED_USTRUCT_BODY()

	/** Higher values reduce component value faster (exponential decay curve) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damping", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float Damping;

	/** Fade component value rate in units / sec */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fade", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float Fade;

	/** Higher values make a sharper appearance, but with more artifacts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacCormack Correction", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float MacCormackBlendFactor;

	/** Minimum absolute value to apply MacCormack correction. Increasing can improve performance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MacCormack Correction", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float MacCormackBlendThreshold;
										
	/** Relative importance of component value for allocation, 0.0 means not important */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Allocation", meta = (ClampMin = 0.0f, UIMin = 0.f, UIMax = 1.0f))
	float AllocWeight;

	/** Minimum component value magnitude that is considered relevant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block Allocation", meta = (ClampMin = 0.0f, UIMin = 0.f, UIMax = 1.0f))
	float AllocThreshold;
};

UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class UFlowMaterial : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Velocity component parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Components)
	FFlowMaterialPerComponent Velocity;

	UPROPERTY()
	FFlowMaterialPerComponent Density_DEPRECATED;

	/** Density component parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Components)
	FFlowMaterialPerComponent Smoke;

	/** Temperature component parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Components)
	FFlowMaterialPerComponent Temperature;

	/** Fuel component parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Components)
	FFlowMaterialPerComponent Fuel;

	/** Higher values increase rotation, reduce laminar flow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticityStrength;

	/** 0.f disabled; 1.0f higher velocities, higher strength; -1.0f for inverse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticityVelocityMask;

	/** 0.f disabled; 1.0f higher temperatures, higher strength; -1.0f for inverse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticityTemperatureMask;

	/** 0.f disabled; 1.0f higher smoke, higher strength; -1.0f for inverse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticitySmokeMask;

	/** 0.f disabled; 1.0f higher fuel, higher strength; -1.0f for inverse */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticityFuelMask;

	/** Works as other masks, provides fixed offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vorticity)
	float VorticityConstantMask;

	/** Minimum temperature for combustion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float IgnitionTemp;

	/** Burn amount per unit temperature above ignitionTemp */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float BurnPerTemp;

	/** Fuel consumed per unit burn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float FuelPerBurn;
	
	/** Temperature increase per unit burn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float TempPerBurn;

	UPROPERTY()
	float DensityPerBurn_DEPRECATED;

	/** Smoke increase per unit burn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float SmokePerBurn;

	/** Expansion per unit burn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float DivergencePerBurn;

	/** Buoyant force per unit temperature */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float BuoyancyPerTemp;

	/** Cooling rate, exponential */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combustion)
	float CoolingRate;

	/** Array of render materials */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	TArray<class UFlowRenderMaterial*> RenderMaterials;

	virtual void PostLoad() override;
};

// NvFlow end
