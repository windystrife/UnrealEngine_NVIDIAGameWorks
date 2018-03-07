// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCollision.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraStats.h"
#include "NiagaraComponent.h"

DECLARE_CYCLE_STAT(TEXT("Collision"), STAT_NiagaraCollision, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Event Emission"), STAT_NiagaraEventWrite, STATGROUP_Niagara);

void FNiagaraCollisionBatch::KickoffNewBatch(FNiagaraEmitterInstance *Sim, float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraCollision);
	FNiagaraVariable PosVar(FNiagaraTypeDefinition::GetVec3Def(), "Position");
	FNiagaraVariable VelVar(FNiagaraTypeDefinition::GetVec3Def(), "Velocity");
	FNiagaraVariable TstVar(FNiagaraTypeDefinition::GetBoolDef(), "PerformCollision");
	FNiagaraDataSetIterator<FVector> PosIt(Sim->GetData(), PosVar, 0, false);
	FNiagaraDataSetIterator<FVector> VelIt(Sim->GetData(), VelVar, 0, false);
	//FNiagaraDataSetIterator<int32> TstIt(Sim->GetData(), TstVar, 0, false);

	if (!PosIt.IsValid() || !VelIt.IsValid() /*|| !TstIt.IsValid()*/)
	{
		return;
	}

	UWorld *SystemWorld = Sim->GetParentSystemInstance()->GetComponent()->GetWorld();
	if (SystemWorld)
	{
		CollisionTraces.Empty();

		for (uint32 i = 0; i < Sim->GetData().GetPrevNumInstances(); i++)
		{
			//int32 TestCollision = *TstIt;
			//if (TestCollision)
			{
				check(PosIt.IsValid() && VelIt.IsValid());
				FVector Position;
				FVector Velocity;
				PosIt.Get(Position);
				VelIt.Get(Velocity);

				FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NiagraAsync));
				QueryParams.OwnerTag = "Niagara";
				FTraceHandle Handle = SystemWorld->AsyncLineTraceByChannel(EAsyncTraceType::Single, Position, Position + Velocity*DeltaSeconds, ECollisionChannel::ECC_WorldStatic, QueryParams, FCollisionResponseParams::DefaultResponseParam, nullptr, i);
				FNiagaraCollisionTrace Trace;
				Trace.CollisionTraceHandle = Handle;
				Trace.SourceParticleIndex = i;
				Trace.OriginalVelocity = Velocity;
				CollisionTraces.Add(Trace);
			}

			//TstIt.Advance();
			PosIt.Advance();
			VelIt.Advance();
		}
	}
}

void FNiagaraCollisionBatch::GenerateEventsFromResults(FNiagaraEmitterInstance *Sim)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraCollision);
	//CollisionEventDataSet.Allocate(CollisionTraceHandles.Num());

	UWorld *SystemWorld = Sim->GetParentSystemInstance()->GetComponent()->GetWorld();
	if (SystemWorld)
	{
		TArray<FNiagaraCollisionEventPayload> Payloads;

		// generate events for last frame's collisions
		//
		for (FNiagaraCollisionTrace CurCheck: CollisionTraces)
		{
			FTraceHandle Handle = CurCheck.CollisionTraceHandle;
			FTraceDatum CurTrace;
			// wait for trace handles; this should block rarely to never
			bool bReady = false;
			while (!bReady)
			{
				bReady = SystemWorld->QueryTraceData(Handle, CurTrace);
				if (!bReady)
				{
					// if the query came back false, it's possible that the hanle is invalid for some reason; skip in that case
					// TODO: handle this more gracefully
					if (!SystemWorld->IsTraceHandleValid(Handle, false))
					{
						break;
					}
					break;
				}
			}

			if (bReady && CurTrace.OutHits.Num())
			{
				// grab the first hit that blocks
				FHitResult *Hit = FHitResult::GetFirstBlockingHit(CurTrace.OutHits);
				if (Hit && Hit->IsValidBlockingHit())
				{
					FNiagaraCollisionEventPayload Event;
					Event.CollisionNormal = Hit->ImpactNormal;
					Event.CollisionPos = Hit->ImpactPoint;
					Event.CollisionVelocity = CurCheck.OriginalVelocity;
					Event.ParticleIndex = CurTrace.UserData;
					check(!Event.CollisionNormal.ContainsNaN());
					check(Event.CollisionNormal.IsNormalized());
					check(!Event.CollisionPos.ContainsNaN());
					check(!Event.CollisionVelocity.ContainsNaN());

					// TODO add to unique list of physical materials for Blueprint
					Event.PhysicalMaterialIndex = 0;// Hit->PhysMaterial->GetUniqueID();

					Payloads.Add(Event);
				}
			}
		}

		if (Payloads.Num())
		{
			// now allocate the data set and write all the event structs
			//
			CollisionEventDataSet->Allocate(Payloads.Num());
			CollisionEventDataSet->SetNumInstances(Payloads.Num());
			//FNiagaraVariable ValidVar(FNiagaraTypeDefinition::GetIntDef(), "Valid");
			FNiagaraVariable PosVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionLocation");
			FNiagaraVariable VelVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionVelocity");
			FNiagaraVariable NormVar(FNiagaraTypeDefinition::GetVec3Def(), "CollisionNormal");
			FNiagaraVariable PhysMatIdxVar(FNiagaraTypeDefinition::GetIntDef(), "PhysicalMaterialIndex");
			FNiagaraVariable ParticleIndexVar(FNiagaraTypeDefinition::GetIntDef(), "ParticleIndex");
			//FNiagaraDataSetIterator<int32> ValidItr(*CollisionEventDataSet, ValidVar, 0, true);
			FNiagaraDataSetIterator<FVector> PosItr(*CollisionEventDataSet, PosVar, 0, true);
			FNiagaraDataSetIterator<FVector> NormItr(*CollisionEventDataSet, NormVar, 0, true);
			FNiagaraDataSetIterator<FVector> VelItr(*CollisionEventDataSet, VelVar, 0, true);
			FNiagaraDataSetIterator<int32> PhysMatItr(*CollisionEventDataSet, PhysMatIdxVar, 0, true);
			FNiagaraDataSetIterator<int32> ParticleIndexItr(*CollisionEventDataSet, ParticleIndexVar, 0, true);

			for (FNiagaraCollisionEventPayload &Payload : Payloads)
			{
				SCOPE_CYCLE_COUNTER(STAT_NiagaraEventWrite);

				check(/*ValidItr.IsValid() && */PosItr.IsValid() && VelItr.IsValid() && NormItr.IsValid() && PhysMatItr.IsValid());
				//ValidItr.Set(1);
				PosItr.Set(Payload.CollisionPos);
				VelItr.Set(Payload.CollisionVelocity);
				NormItr.Set(Payload.CollisionNormal);
				ParticleIndexItr.Set(Payload.ParticleIndex);
				PhysMatItr.Set(0);
				//ValidItr.Advance();
				PosItr.Advance();
				VelItr.Advance();
				NormItr.Advance();
				PhysMatItr.Advance();
				ParticleIndexItr.Advance();
			}
		}
		else
		{
			CollisionEventDataSet->SetNumInstances(0);
		}

	}
}