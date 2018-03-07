#include "FlowMaterial.h"

#include "NvFlowCommon.h"
#include "FlowRenderMaterial.h"

// NvFlow begin

namespace
{
	void CopyMaterialPerComponent(const NvFlowGridMaterialPerComponent& In, FFlowMaterialPerComponent& Out)
	{
		Out.Damping = In.damping;
		Out.Fade = In.fade;
		Out.MacCormackBlendFactor = In.macCormackBlendFactor;
		Out.MacCormackBlendThreshold = In.macCormackBlendThreshold;
		Out.AllocWeight = In.allocWeight;
		Out.AllocThreshold = In.allocThreshold;
	}
}


static const FFlowMaterialPerComponent PerComponentDensity_DEPRECATED_Default = { 0.1f, 0.1f, 0.5f, 0.001f, 0.0f, 0.0f };
static const float DensityPerBurn_DEPRECATED_Default = 3.0f;

bool operator != (const FFlowMaterialPerComponent& A, const FFlowMaterialPerComponent& B)
{
	return (A.Damping != B.Damping || A.Fade != B.Fade ||
		A.MacCormackBlendFactor != B.MacCormackBlendFactor || A.MacCormackBlendThreshold != B.MacCormackBlendThreshold ||
		A.AllocWeight != B.AllocWeight || A.AllocThreshold != B.AllocThreshold);
}


UFlowMaterial::UFlowMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Density_DEPRECATED(PerComponentDensity_DEPRECATED_Default)
	, DensityPerBurn_DEPRECATED(DensityPerBurn_DEPRECATED_Default)
{
	NvFlowGridMaterialParams FlowGridMaterialParams;
	NvFlowGridMaterialParamsDefaultsInline(&FlowGridMaterialParams);

	CopyMaterialPerComponent(FlowGridMaterialParams.velocity, Velocity);
	CopyMaterialPerComponent(FlowGridMaterialParams.smoke, Smoke);
	CopyMaterialPerComponent(FlowGridMaterialParams.temperature, Temperature);
	CopyMaterialPerComponent(FlowGridMaterialParams.fuel, Fuel);

	VorticityStrength = FlowGridMaterialParams.vorticityStrength;
	VorticityVelocityMask = FlowGridMaterialParams.vorticityVelocityMask;
	VorticityTemperatureMask = FlowGridMaterialParams.vorticityTemperatureMask;
	VorticitySmokeMask = FlowGridMaterialParams.vorticitySmokeMask;
	VorticityFuelMask = FlowGridMaterialParams.vorticityFuelMask;
	VorticityConstantMask = FlowGridMaterialParams.vorticityConstantMask;
	IgnitionTemp = FlowGridMaterialParams.ignitionTemp;
	BurnPerTemp = FlowGridMaterialParams.burnPerTemp;
	FuelPerBurn = FlowGridMaterialParams.fuelPerBurn;
	TempPerBurn = FlowGridMaterialParams.tempPerBurn;
	SmokePerBurn = FlowGridMaterialParams.smokePerBurn;
	DivergencePerBurn = FlowGridMaterialParams.divergencePerBurn;
	BuoyancyPerTemp = FlowGridMaterialParams.buoyancyPerTemp;
	CoolingRate = FlowGridMaterialParams.coolingRate;

	UFlowRenderMaterial* DefaultRenderMaterial = CreateDefaultSubobject<UFlowRenderMaterial>(TEXT("DefaultFlowRenderMaterial0"));
	RenderMaterials.Add(DefaultRenderMaterial);
}

void UFlowMaterial::PostLoad()
{
	Super::PostLoad();

	if (Density_DEPRECATED != PerComponentDensity_DEPRECATED_Default)
	{
		Smoke = Density_DEPRECATED;
	}
	if (DensityPerBurn_DEPRECATED != DensityPerBurn_DEPRECATED_Default)
	{
		SmokePerBurn = DensityPerBurn_DEPRECATED;
	}
}

// NvFlow end
