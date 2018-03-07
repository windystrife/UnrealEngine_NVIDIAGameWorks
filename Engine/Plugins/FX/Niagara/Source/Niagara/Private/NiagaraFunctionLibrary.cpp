// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraFunctionLibrary.h"
#include "EngineGlobals.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "NiagaraWorldManager.h"

UNiagaraFunctionLibrary::UNiagaraFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}


/**
* Spawns a Niagara System at the specified world location/rotation
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAtLocation(UObject* WorldContextObject, UNiagaraSystem* SystemTemplate, FVector SpawnLocation, FRotator SpawnRotation, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = NULL;
	if (SystemTemplate)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World != nullptr)
		{
			AActor* Actor = World->GetWorldSettings();
			PSC = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)World));
			PSC->SetAsset(SystemTemplate);
			PSC->RegisterComponentWithWorld(World);

			PSC->SetAbsolute(true, true, true);
			PSC->SetWorldLocationAndRotation(SpawnLocation, SpawnRotation);
			PSC->SetRelativeScale3D(FVector(1.f));
			//PSC->ActivateSystem(true);
		}
	}
	return PSC;
}





/**
* Spawns a Niagara System attached to a component
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttached(UNiagaraSystem* SystemTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy)
{
	UNiagaraComponent* PSC = nullptr;
	if (SystemTemplate)
	{
		if (AttachToComponent == NULL)
		{
			UE_LOG(LogScript, Warning, TEXT("UNiagaraFunctionLibrary::SpawnSystemAttached: NULL AttachComponent specified!"));
		}
		else
		{
			AActor* Actor = AttachToComponent->GetOwner();
			PSC = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)AttachToComponent->GetWorld()));
			PSC->SetAsset(SystemTemplate);
			PSC->RegisterComponentWithWorld(AttachToComponent->GetWorld());

			PSC->AttachToComponent(AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, AttachPointName);
			if (LocationType == EAttachLocation::KeepWorldPosition)
			{
				PSC->SetWorldLocationAndRotation(Location, Rotation);
			}
			else
			{
				PSC->SetRelativeLocationAndRotation(Location, Rotation);
			}
			PSC->SetRelativeScale3D(FVector(1.f));
		}
	}
	return PSC;
}


/**
* Set a constant in an emitter of a Niagara System
void UNiagaraFunctionLibrary::SetUpdateScriptConstant(UNiagaraComponent* Component, FName EmitterName, FName ConstantName, FVector Value)
{
	TArray<TSharedPtr<FNiagaraEmitterInstance>> &Emitters = Component->GetSystemInstance()->GetEmitters();

	for (TSharedPtr<FNiagaraEmitterInstance> &Emitter : Emitters)
	{		
		if(UNiagaraEmitter* PinnedProps = Emitter->GetProperties().Get())
		{
			FName CurName = *PinnedProps->EmitterName;
			if (CurName == EmitterName)
			{
				Emitter->GetProperties()->UpdateScriptProps.ExternalConstants.SetOrAdd(FNiagaraTypeDefinition::GetVec4Def(), ConstantName, Value);
				break;
			}
		}
	}
}
*/

UNiagaraParameterCollectionInstance* UNiagaraFunctionLibrary::GetNiagaraParameterCollection(UObject* WorldContextObject, UNiagaraParameterCollection* Collection)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		return FNiagaraWorldManager::Get(World)->GetParameterCollection(Collection);
	}
	return nullptr;
}
