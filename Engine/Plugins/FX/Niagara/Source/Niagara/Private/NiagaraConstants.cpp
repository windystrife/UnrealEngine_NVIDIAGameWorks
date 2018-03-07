// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraConstants.h"

#include "Internationalization.h"

#define LOCTEXT_NAMESPACE "FNiagaraConstants"

const TArray<FNiagaraVariable>& FNiagaraConstants::GetEngineConstants()
{
	static TArray<FNiagaraVariable> SystemParameters;
	if (SystemParameters.Num() == 0)
	{
		SystemParameters.Add(SYS_PARAM_ENGINE_DELTA_TIME);
		SystemParameters.Add(SYS_PARAM_ENGINE_INV_DELTA_TIME);
		SystemParameters.Add(SYS_PARAM_ENGINE_POSITION);
		SystemParameters.Add(SYS_PARAM_ENGINE_VELOCITY);
		SystemParameters.Add(SYS_PARAM_ENGINE_X_AXIS);
		SystemParameters.Add(SYS_PARAM_ENGINE_Y_AXIS);
		SystemParameters.Add(SYS_PARAM_ENGINE_Z_AXIS);
		SystemParameters.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
		SystemParameters.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
		SystemParameters.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
		SystemParameters.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
		SystemParameters.Add(SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA);
		SystemParameters.Add(SYS_PARAM_ENGINE_EXEC_COUNT);
		SystemParameters.Add(SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES);
		SystemParameters.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE);
		SystemParameters.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS);
	}
	return SystemParameters;
}

FNiagaraVariable FNiagaraConstants::UpdateEngineConstant(const FNiagaraVariable& InVar)
{
	const FNiagaraVariable* FoundSystemVar = FindEngineConstant(InVar);
	if (nullptr != FoundSystemVar)
	{
		return *FoundSystemVar;
	}
	else
	{
		static TMap<FName, FNiagaraVariable> UpdatedSystemParameters;
		if (UpdatedSystemParameters.Num() == 0)
		{
			UpdatedSystemParameters.Add(FName(TEXT("System Delta Time")), SYS_PARAM_ENGINE_DELTA_TIME);
			UpdatedSystemParameters.Add(FName(TEXT("System Inv Delta Time")), SYS_PARAM_ENGINE_INV_DELTA_TIME);
			UpdatedSystemParameters.Add(FName(TEXT("System Position")), SYS_PARAM_ENGINE_POSITION);
			UpdatedSystemParameters.Add(FName(TEXT("System Velocity")), SYS_PARAM_ENGINE_VELOCITY);
			UpdatedSystemParameters.Add(FName(TEXT("System X Axis")), SYS_PARAM_ENGINE_X_AXIS);
			UpdatedSystemParameters.Add(FName(TEXT("System Y Axis")), SYS_PARAM_ENGINE_Y_AXIS);
			UpdatedSystemParameters.Add(FName(TEXT("System Z Axis")), SYS_PARAM_ENGINE_Z_AXIS);
			UpdatedSystemParameters.Add(FName(TEXT("System Local To World")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
			UpdatedSystemParameters.Add(FName(TEXT("System World To Local")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
			UpdatedSystemParameters.Add(FName(TEXT("System Local To World Transposed")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
			UpdatedSystemParameters.Add(FName(TEXT("System World To Local Transposed")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);

			UpdatedSystemParameters.Add(FName(TEXT("Emitter Execution Count")), SYS_PARAM_ENGINE_EXEC_COUNT);
			UpdatedSystemParameters.Add(FName(TEXT("Emitter Age")), SYS_PARAM_EMITTER_AGE);
			UpdatedSystemParameters.Add(FName(TEXT("Emitter Spawn Rate")), SYS_PARAM_EMITTER_SPAWNRATE);
			UpdatedSystemParameters.Add(FName(TEXT("Emitter Spawn Interval")), SYS_PARAM_EMITTER_SPAWN_INTERVAL);
			UpdatedSystemParameters.Add(FName(TEXT("Emitter Interp Spawn Start Dt")), SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT);

			UpdatedSystemParameters.Add(FName(TEXT("Delta Time")), SYS_PARAM_ENGINE_DELTA_TIME);
			UpdatedSystemParameters.Add(FName(TEXT("Emitter Age")), SYS_PARAM_EMITTER_AGE);
			UpdatedSystemParameters.Add(FName(TEXT("Effect Position")), SYS_PARAM_ENGINE_POSITION);
			UpdatedSystemParameters.Add(FName(TEXT("Effect Velocity")), SYS_PARAM_ENGINE_VELOCITY);
			UpdatedSystemParameters.Add(FName(TEXT("Effect X Axis")), SYS_PARAM_ENGINE_X_AXIS);
			UpdatedSystemParameters.Add(FName(TEXT("Effect Y Axis")), SYS_PARAM_ENGINE_Y_AXIS);
			UpdatedSystemParameters.Add(FName(TEXT("Effect Z Axis")), SYS_PARAM_ENGINE_Z_AXIS);

			UpdatedSystemParameters.Add(FName(TEXT("Effect Local To World")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
			UpdatedSystemParameters.Add(FName(TEXT("Effect World To Local")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
			UpdatedSystemParameters.Add(FName(TEXT("Effect Local To World Transposed")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
			UpdatedSystemParameters.Add(FName(TEXT("Effect World To Local Transposed")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
			UpdatedSystemParameters.Add(FName(TEXT("Execution Count")), SYS_PARAM_ENGINE_EXEC_COUNT);
			UpdatedSystemParameters.Add(FName(TEXT("Spawn Rate")), SYS_PARAM_EMITTER_SPAWNRATE);
			UpdatedSystemParameters.Add(FName(TEXT("Spawn Interval")), SYS_PARAM_EMITTER_SPAWN_INTERVAL);
			UpdatedSystemParameters.Add(FName(TEXT("Interp Spawn Start Dt")), SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT);
			UpdatedSystemParameters.Add(FName(TEXT("Inv Delta Time")), SYS_PARAM_ENGINE_INV_DELTA_TIME);
		}

		const FNiagaraVariable* FoundSystemVarUpdate = UpdatedSystemParameters.Find(InVar.GetName());
		if (FoundSystemVarUpdate != nullptr)
		{
			return *FoundSystemVarUpdate;
		}
	}
	return InVar;

}


const FNiagaraVariable* FNiagaraConstants::FindEngineConstant(const FNiagaraVariable& InVar)
{
	const TArray<FNiagaraVariable>& SystemParameters = GetEngineConstants();
	const FNiagaraVariable* FoundSystemVar = SystemParameters.FindByPredicate([&](const FNiagaraVariable& Var)
	{
		return Var.GetName() == InVar.GetName();
	});
	return FoundSystemVar;
}

FText FNiagaraConstants::GetEngineConstantDescription(const FNiagaraVariable& InAttribute)
{
	static TMap<FNiagaraVariable, FText> AttrStrMap;
	if (AttrStrMap.Num() == 0)
	{
		AttrStrMap.Add(SYS_PARAM_ENGINE_DELTA_TIME, LOCTEXT("EngineDeltaTimeDesc", "Time in seconds since the last tick."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_INV_DELTA_TIME, LOCTEXT("EngineInvDeltaTimeDesc", "One over Engine.DeltaTime"));
		AttrStrMap.Add(SYS_PARAM_ENGINE_POSITION, LOCTEXT("EnginePositionDesc", "The owning component's position in world space."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_VELOCITY, LOCTEXT("EngineVelocityDesc", "The owning component's velocity in world space."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_X_AXIS, LOCTEXT("XAxisDesc", "The X-axis of the owning component."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_Y_AXIS, LOCTEXT("YAxisDesc", "The Y-axis of the owning component."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_Z_AXIS, LOCTEXT("ZAxisDesc", "The Z-axis of the owning component."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD, LOCTEXT("LocalToWorldDesc", "Conversion of the owning component's local to world space."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL, LOCTEXT("WorldToLocalDesc", "Conversion of the owning component's world to local space."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED, LOCTEXT("LocalToWorldTransposeDesc", "Conversion of the owning component's local to world space transposed."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED, LOCTEXT("WorldToLocalTransposeDesc", "Conversion of the owning component's world to local space transposed."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA, LOCTEXT("MinDistanceToCamera", "The distance from the owner component to the nearest local player viewpoint."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_EXEC_COUNT, LOCTEXT("ExecCountDesc", "The index of this particle in the read buffer."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES, LOCTEXT("EmitterNumParticles", "The number of particles for this emitter at the beginning of simulation. Should only be used in Emitter scripts."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE, LOCTEXT("SystemNumEmittersAlive", "The number of emitters still alive attached to this system. Should only be used in System scripts."));
		AttrStrMap.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS, LOCTEXT("SystemNumEmitters", "The number of emitters attached to this system. Should only be used in System scripts."));
	}

	FText* FoundStr = AttrStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FText();
}

const TArray<FNiagaraVariable>& FNiagaraConstants::GetCommonParticleAttributes()
{
	static TArray<FNiagaraVariable> Attributes;
	if (Attributes.Num() == 0)
	{
		Attributes.Add(SYS_PARAM_PARTICLES_POSITION);
		Attributes.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attributes.Add(SYS_PARAM_PARTICLES_COLOR);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attributes.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		Attributes.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		Attributes.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		Attributes.Add(SYS_PARAM_PARTICLES_SCALE);
		Attributes.Add(SYS_PARAM_PARTICLES_ROTATION);

		Attributes.Add(SYS_PARAM_INSTANCE_ALIVE);
	}
	return Attributes;
}

FString FNiagaraConstants::GetAttributeDefaultValue(const FNiagaraVariable& InAttribute)
{
	static TMap<FNiagaraVariable, FString> AttrStrMap;
	if (AttrStrMap.Num() == 0)
	{
		AttrStrMap.Add(SYS_PARAM_PARTICLES_POSITION, SYS_PARAM_ENGINE_POSITION.GetName().ToString());
		AttrStrMap.Add(SYS_PARAM_PARTICLES_VELOCITY, TEXT("0.0,0.0,100.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_COLOR, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f).ToString());
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, TEXT("0.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, TEXT("0.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, TEXT("X=50.0 Y=50.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, TEXT("1.0,0.0,0.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, TEXT("1.0,0.0,0.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, TEXT("0.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, TEXT("1.0,1.0,1.0,1.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SCALE, TEXT("1.0,1.0,1.0"));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_ROTATION, TEXT("0.0,0.0,0.0"));
	}

	FString* FoundStr = AttrStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FString();
}

FText FNiagaraConstants::GetAttributeDescription(const FNiagaraVariable& InAttribute)
{
	static TMap<FNiagaraVariable, FText> AttrStrMap;
	if (AttrStrMap.Num() == 0)
	{
		AttrStrMap.Add(SYS_PARAM_PARTICLES_POSITION, LOCTEXT("PositionDesc", "The position of the particle."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_VELOCITY, LOCTEXT("VelocityDesc", "The velocity in cm/s of the particle."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_COLOR, LOCTEXT("ColorDesc", "The color of the particle."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, LOCTEXT("SpriteRotDesc", "The screen aligned roll of the particle."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, LOCTEXT("NormalizedAgeDesc", "The age in seconds divided by lifetime in seconds. Useful for animation as the value is between 0 and 1."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, LOCTEXT("SpriteSizeDesc", "The size of the sprite quad."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, LOCTEXT("FacingDesc", "Makes the surface of the sprite face towards a custom vector."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, LOCTEXT("AlignmentDesc", "Imagine the texture having an arrow to the right, this attribute makes the arrow point towards the alignment axis."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, LOCTEXT("SubImageIndexDesc", "The position in the UV lookup table divided by total number of UV slots."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, LOCTEXT("DynamicMaterialParameterDesc", "The 4-float vector used to send custom data to renderer."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_SCALE, LOCTEXT("ScaleParamDesc", "The XYZ scale of the non-sprite based particle."));
		AttrStrMap.Add(SYS_PARAM_PARTICLES_ROTATION, LOCTEXT("RotationParamDesc", "The Vector3 euler angle rotation"));
		AttrStrMap.Add(SYS_PARAM_INSTANCE_ALIVE, LOCTEXT("AliveParamDesc", "Used to determine whether or not this particle instance is still valid or if it can be deleted."));
	}

	FText* FoundStr = AttrStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE