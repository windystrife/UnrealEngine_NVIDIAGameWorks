// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODProxyProcessor.h"
#include "Misc/ScopeLock.h"
#include "Engine/StaticMesh.h"
#include "Engine/LODActor.h"
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "Editor.h"

#include "Interfaces/IProjectManager.h"
#include "StaticMeshResources.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

FHierarchicalLODProxyProcessor::FHierarchicalLODProxyProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.AddRaw(this, &FHierarchicalLODProxyProcessor::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddRaw(this, &FHierarchicalLODProxyProcessor::OnNewCurrentLevel);
#endif // WITH_EDITOR
}

FHierarchicalLODProxyProcessor::~FHierarchicalLODProxyProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
#endif // WITH_EDITOR
}

bool FHierarchicalLODProxyProcessor::Tick(float DeltaTime)
{
	FScopeLock Lock(&StateLock);

	while (ToProcessJobs.Num())
	{
		FProcessData* Data = ToProcessJobs.Pop();
		UStaticMesh* MainMesh = nullptr;		
		for (UObject* AssetObject : Data->AssetObjects)
		{
			// Check if this is the generated proxy (static-)mesh
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetObject);
			if (StaticMesh)
			{
				check(StaticMesh != nullptr);
				MainMesh = StaticMesh;
			}
		}
		check(MainMesh != nullptr);

		// Force lightmap coordinate to 0 for proxy meshes
		MainMesh->LightMapCoordinateIndex = 0;
		// Trigger post edit change, simulating we made a change in the Static mesh editor (could only call Build, but this is for possible future changes)
		MainMesh->PostEditChange();

		// Set new static mesh, location and sub-objects (UObjects)
		Data->LODActor->SetStaticMesh(MainMesh);
		Data->LODActor->SetActorLocation(FVector::ZeroVector);
		Data->LODActor->SubObjects = Data->AssetObjects;

		// Check resulting mesh and give a warning if it exceeds the vertex / triangle cap for certain platforms
		FProjectStatus ProjectStatus;
		if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus) && (ProjectStatus.IsTargetPlatformSupported(TEXT("Android")) || ProjectStatus.IsTargetPlatformSupported(TEXT("IOS"))))
		{
			if (MainMesh->RenderData.IsValid() && MainMesh->RenderData->LODResources.Num() && MainMesh->RenderData->LODResources[0].IndexBuffer.Is32Bit())
			{
				FMessageLog("HLODResults").Warning()
					->AddToken(FUObjectToken::Create(Data->LODActor))
					->AddToken(FTextToken::Create(FText::FromString(TEXT(" Mesh has more that 65535 vertices, incompatible with mobile; forcing 16-bit (will probably cause rendering issues)."))));
				
				FMessageLog("HLODResults").Open();
			}
		}

		// Calculate correct drawing distance according to given screensize
		// At the moment this assumes a fixed field of view of 90 degrees (horizontal and vertical axi)
		static const float FOVRad = 90.0f * (float)PI / 360.0f;
		static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
		FBoxSphereBounds Bounds = Data->LODActor->GetStaticMeshComponent()->CalcBounds(FTransform());

		FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
		IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
		Data->LODActor->LODDrawDistance = Utilities->CalculateDrawDistanceFromScreenSize(Bounds.SphereRadius, Data->LODSetup.TransitionScreenSize, ProjectionMatrix);
		Data->LODActor->UpdateSubActorLODParents();

		// Freshly build so mark not dirty
		Data->LODActor->SetIsDirty(false);
		delete Data;
	}

	return true;
}

FGuid FHierarchicalLODProxyProcessor::AddProxyJob(ALODActor* InLODActor, const FHierarchicalSimplification& LODSetup)
{
	FScopeLock Lock(&StateLock);
	check(InLODActor);
	// Create new unique Guid which will be used to identify this job
	FGuid JobGuid = FGuid::NewGuid();
	// Set up processing data
	FProcessData* Data = new FProcessData();
	Data->LODActor = InLODActor;
	Data->LODSetup = LODSetup;	

	JobActorMap.Add(JobGuid, Data);

	return JobGuid;
}

void FHierarchicalLODProxyProcessor::ProcessProxy(const FGuid InGuid, TArray<UObject*>& InAssetsToSync)
{
	FScopeLock Lock(&StateLock);

	// Check if there is data stored for the given Guid
	FProcessData** DataPtr = JobActorMap.Find(InGuid);
	if (DataPtr)
	{
		// If so push the job onto the processing queue
		FProcessData* Data = *DataPtr;
		JobActorMap.Remove(InGuid);
		if (Data && Data->LODActor)
		{
			Data->AssetObjects = InAssetsToSync;
			ToProcessJobs.Push(Data);
		}
	}
}

FCreateProxyDelegate& FHierarchicalLODProxyProcessor::GetCallbackDelegate()
{
	// Bind ProcessProxy to the delegate (if not bound yet)
	if (!CallbackDelegate.IsBound())
	{
		CallbackDelegate.BindRaw(this, &FHierarchicalLODProxyProcessor::ProcessProxy);
	}

	return CallbackDelegate;
}

void FHierarchicalLODProxyProcessor::OnMapChange(uint32 MapFlags)
{
	ClearProcessingData();
}

void FHierarchicalLODProxyProcessor::OnNewCurrentLevel()
{
	ClearProcessingData();
}

void FHierarchicalLODProxyProcessor::ClearProcessingData()
{
	FScopeLock Lock(&StateLock);
	JobActorMap.Empty();
	ToProcessJobs.Empty();
}
