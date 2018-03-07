// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleModules_Flex.cpp: 
	Flex-related particle module implementations.
=============================================================================*/

#include "Particles/Modules/Flex/ParticleModuleFlexShapeSpawn.h"
#include "Particles/Modules/Flex/ParticleModuleFlexFluidSpawn.h"

#include "PhysicsEngine/FlexContainer.h"
#include "PhysicsEngine/FlexAsset.h"

/*-----------------------------------------------------------------------------
UParticleModuleFlexShapeSpawn implementation.
-----------------------------------------------------------------------------*/

UParticleModuleFlexShapeSpawn::UParticleModuleFlexShapeSpawn(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bSpawnModule = true;
	bSupported3DDrawMode = false;

	Mesh = NULL;
	Velocity = 0.0f;
}

bool UParticleModuleFlexShapeSpawn::GetSpawnAmount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& Rate)
{
	return false;
}

bool UParticleModuleFlexShapeSpawn::GetBurstCount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number)
{
	if (Mesh && Mesh->FlexAsset)
	{
		Number = Mesh->FlexAsset->Particles.Num();
		return true;
	}

	return false;
}

void UParticleModuleFlexShapeSpawn::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
}

void UParticleModuleFlexShapeSpawn::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;

	if (Mesh && Mesh->FlexAsset)
	{
		// calculate spawn position based on the particle counter
		int32 ParticlesPerShape = Mesh->FlexAsset->Particles.Num();
		int32 ParticleShapeIndex = Owner->ParticleCounter%ParticlesPerShape;

		FVector Vel = Owner->EmitterToSimulation.TransformVector(FVector(0.0f, 0.0f, Velocity));

		Particle.Location += Owner->EmitterToSimulation.TransformVector(FVector(Mesh->FlexAsset->Particles[ParticleShapeIndex]));
		Particle.Velocity += Vel;
		Particle.BaseVelocity += Vel;
	}
}

int32 UParticleModuleFlexShapeSpawn::GetMaximumBurstCount()
{
	if (Mesh && Mesh->FlexAsset)
		return Mesh->FlexAsset->Particles.Num();
	else
		return 0;
}


/*-----------------------------------------------------------------------------
	UParticleModuleFlexFluidSpawn implementation.
-----------------------------------------------------------------------------*/

UParticleModuleFlexFluidSpawn::UParticleModuleFlexFluidSpawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSpawnModule = true;
	bSupported3DDrawMode = false;
}


void UParticleModuleFlexFluidSpawn::InitializeDefaults()
{
	if (!Velocity.Distribution)
	{
		UDistributionFloatConstant* VelDist = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionVelocity"));	
		VelDist->Constant = 200.0f;
		Velocity.Distribution = VelDist;
	}

	if (!DimX.Distribution)	
	{
		UDistributionFloatConstant* DimXDist = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionDimX"));
		DimXDist->Constant = 4;
		DimX.Distribution = DimXDist;
	}

	if (!DimY.Distribution)
	{ 
		UDistributionFloatConstant* DimYDist = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionDimY"));
		DimYDist->Constant = 4;
		DimY.Distribution = DimYDist;
	}

	if (!LayerScale.Distribution)
	{
		UDistributionFloatConstant* LayerScaleDist = NewObject<UDistributionFloatConstant>(this, TEXT("DistributionLayerScale"));
		LayerScaleDist->Constant = 1.0f;
		LayerScale.Distribution = LayerScaleDist;
	}

}

void UParticleModuleFlexFluidSpawn::PostLoad()
{
	Super::PostLoad();

	// need to initialize in case this is an old emitter (serialized before switching to distributions)
	InitializeDefaults();
}

void UParticleModuleFlexFluidSpawn::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		InitializeDefaults();
	}
}

void UParticleModuleFlexFluidSpawn::CompileModule(FParticleEmitterBuildInfo& EmitterInfo)
{
	/*
	FVector AxisScaleMask(
		MultiplyX ? 1.0f : 0.0f,
		MultiplyY ? 1.0f : 0.0f,
		MultiplyZ ? 1.0f : 0.0f
		);
	FVector AxisKeepMask(
		1.0f - AxisScaleMask.X,
		1.0f - AxisScaleMask.Y,
		1.0f - AxisScaleMask.Z
		);
	EmitterInfo.SizeScale.Initialize(LifeMultiplier.Distribution);
	EmitterInfo.SizeScale.ScaleByConstantVector(AxisScaleMask);
	EmitterInfo.SizeScale.AddConstantVector(AxisKeepMask);
	*/
}

#if WITH_EDITOR
void UParticleModuleFlexFluidSpawn::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeDefaults();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

bool UParticleModuleFlexFluidSpawn::GetSpawnAmount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& Number, float& Rate)
{
	return false;
}

bool UParticleModuleFlexFluidSpawn::GetBurstCount(FParticleEmitterInstance* Owner, int32 Offset, float OldLeftover, float DeltaTime, int32& ReturnNumber)
{
	// how many layers we need to emit
	InstancePayload& Payload = *((InstancePayload*)Owner->GetModuleInstanceData(this));

	UFlexContainer* Template = Owner->SpriteTemplate->FlexContainerTemplate;

	if (Template)
	{
		// ensure a constant spacing between layers
		float Spacing = Template->Radius;

		if (Template->Fluid)
			Spacing *= Template->RestDistance;

		float VelocityVal = Velocity.Distribution->GetValue(0.0f, Owner->Component);
		float DimXVal = DimX.Distribution->GetValue(0.0f, Owner->Component);
		float DimYVal = DimY.Distribution->GetValue(0.0f, Owner->Component);
		float LayerScaleVal = LayerScale.Distribution->GetValue(0.0f, Owner->Component);

		// clamp delta time like flex sim does
		// compute max left over, formula from flex sim. 60 should likely be replaced with template->framerate.
		DeltaTime = FMath::Min(DeltaTime, 1.0f / float(Template->MinFrameRate));
		const float StepsPerSecond = Template->NumSubsteps*60.0f;
		const float SubstepDt = 1.0f / StepsPerSecond;
		const float ElapsedTime = Payload.TimeLeftOver + DeltaTime;
		float Dt;
		if (Template->FixedTimeStep)
		{
			int32 NumSubsteps = int32(ElapsedTime / SubstepDt);
			Dt = NumSubsteps*SubstepDt;
			// don't carry over more than 1 substep worth of time
			Payload.TimeLeftOver = FMath::Min(ElapsedTime - Dt, SubstepDt);
		}
		else
		{
			int32 NumSubsteps = Template->NumSubsteps;
			Dt = DeltaTime;
			Payload.TimeLeftOver = 0.0f;
		}

		float LayersPerSecond = VelocityVal / Spacing;
		float LayerCount = Payload.LayerLeftOver + LayersPerSecond*Dt*LayerScaleVal;
		int32 NumLayers = int32(LayerCount);
		ReturnNumber = int32(DimXVal)*int32(DimYVal)*NumLayers;

		Payload.LayerLeftOver = FMath::Min(LayerCount - NumLayers, 1.0f);
		Payload.NumParticles = ReturnNumber;
		Payload.ParticleIndex = 0;
	}

	return true;
}

void UParticleModuleFlexFluidSpawn::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
}

void UParticleModuleFlexFluidSpawn::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;

	UFlexContainer* Template = Owner->SpriteTemplate->FlexContainerTemplate;
	InstancePayload& Payload = *((InstancePayload*)Owner->GetModuleInstanceData(this));
	
	if (Template)
	{
		// ensure a constant spacing between layers
		float Spacing = Template->Radius;

		if (Template->Fluid)
			Spacing *= Template->RestDistance;

		float DimXVal = DimX.Distribution->GetValue(0.0f, Owner->Component);
		float DimYVal = DimY.Distribution->GetValue(0.0f, Owner->Component);
		float VelocityVal = Velocity.Distribution->GetValue(0.0f, Owner->Component);

		int32 IntDimX = int32(DimXVal);
		int32 IntDimY = int32(DimYVal);

		// calculate spawn position based on the particle counter
		int32 ParticlesPerLayer = IntDimX*IntDimY;
		int32 LayerIndex = Payload.ParticleIndex % ParticlesPerLayer;
		int32 Layer = Payload.ParticleIndex / ParticlesPerLayer;

		int32 X = LayerIndex%IntDimX;
		int32 Y = LayerIndex/IntDimX;

		FVector Vel = Owner->EmitterToSimulation.TransformVector(FVector(0.0f, 0.0f, VelocityVal));
		FVector2D Center(Spacing*IntDimX*0.5f, Spacing*IntDimY*0.5f);

		Particle.Location += Owner->EmitterToSimulation.TransformVector(FVector(X*Spacing - Center.X, Y*Spacing - Center.Y, Layer*Spacing));
		Particle.Velocity += Vel;
		Particle.BaseVelocity += Vel;
		// disable particle and shadow rendering in first frame
		Particle.Size = FVector(0.0f);
		Payload.ParticleIndex++;
	}	
}

int32 UParticleModuleFlexFluidSpawn::GetMaximumBurstCount()
{
	return int32(DimX.Distribution->GetValue()*DimY.Distribution->GetValue());
}

uint32 UParticleModuleFlexFluidSpawn::RequiredBytesPerInstance()
{
	return sizeof(InstancePayload);
}

uint32 UParticleModuleFlexFluidSpawn::PrepPerInstanceBlock(FParticleEmitterInstance* Owner, void* InstData)
{
	InstancePayload& PayLoad = *((InstancePayload*)InstData);

	PayLoad.LayerLeftOver = 0.0f;
	PayLoad.NumParticles = 0;
	PayLoad.ParticleIndex = 0;
	PayLoad.TimeLeftOver = 0.0f;

	return 0;
}

