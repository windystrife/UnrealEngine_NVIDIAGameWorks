// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProxyGenerationProcessor.h"
#include "MaterialUtilities.h"
#include "MeshUtilities.h"
#include "ProxyMaterialUtilities.h"
#include "IMeshReductionInterfaces.h"

#include "IMeshReductionManagerModule.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

FProxyGenerationProcessor::FProxyGenerationProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.AddRaw(this, &FProxyGenerationProcessor::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddRaw(this, &FProxyGenerationProcessor::OnNewCurrentLevel);
	
	IMeshReductionManagerModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	IMeshMerging* MeshMerging = Module.GetMeshMergingInterface();
	if (!MeshMerging)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("No automatic mesh merging module available"));
	}
	else
	{
		MeshMerging->CompleteDelegate.BindRaw(this, &FProxyGenerationProcessor::ProxyGenerationComplete);
		MeshMerging->FailedDelegate.BindRaw(this, &FProxyGenerationProcessor::ProxyGenerationFailed);
	}

	IMeshMerging* DistributedMeshMerging = Module.GetDistributedMeshMergingInterface();
	if (!DistributedMeshMerging)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("No distributed automatic mesh merging module available"));
	}
	else
	{
		DistributedMeshMerging->CompleteDelegate.BindRaw(this, &FProxyGenerationProcessor::ProxyGenerationComplete);
		DistributedMeshMerging->FailedDelegate.BindRaw(this, &FProxyGenerationProcessor::ProxyGenerationFailed);
	}
#endif // WITH_EDITOR
}

FProxyGenerationProcessor::~FProxyGenerationProcessor()
{
#if WITH_EDITOR
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
#endif // WITH_EDITOR
}

void FProxyGenerationProcessor::AddProxyJob(FGuid InJobGuid, FMergeCompleteData* InCompleteData)
{
	FScopeLock Lock(&StateLock);
	ProxyMeshJobs.Add(InJobGuid, InCompleteData);
}

bool FProxyGenerationProcessor::Tick(float DeltaTime)
{
	FScopeLock Lock(&StateLock);
	for (const auto& Entry : ToProcessJobDataMap)
	{
		FGuid JobGuid = Entry.Key;
		FProxyGenerationData* Data = Entry.Value;

		// Process the job
		ProcessJob(JobGuid, Data);

		// Data retrieved so can now remove the job from the map
		ProxyMeshJobs.Remove(JobGuid);
		delete Data->MergeData;
		delete Data;
	}

	ToProcessJobDataMap.Reset();

	return true;
}

void FProxyGenerationProcessor::ProxyGenerationComplete(FRawMesh& OutProxyMesh, struct FFlattenMaterial& OutMaterial, const FGuid OutJobGUID)
{
	FScopeLock Lock(&StateLock);
	FMergeCompleteData** FindData = ProxyMeshJobs.Find(OutJobGUID);
	if (FindData && *FindData)
	{
		FMergeCompleteData* Data = *FindData;

		FProxyGenerationData* GenerationData = new FProxyGenerationData();
		GenerationData->Material = OutMaterial;
		GenerationData->RawMesh = OutProxyMesh;
		GenerationData->MergeData = Data;

		ToProcessJobDataMap.Add(OutJobGUID, GenerationData);
	}
}

void FProxyGenerationProcessor::ProxyGenerationFailed(const FGuid OutJobGUID, const FString& ErrorMessage)
{
	FScopeLock Lock(&StateLock);
	FMergeCompleteData** FindData = ProxyMeshJobs.Find(OutJobGUID);
	if (FindData && *FindData)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("Failed to generate proxy mesh for cluster %s, %s"), *(*FindData)->ProxyBasePackageName, *ErrorMessage);
		ProxyMeshJobs.Remove(OutJobGUID);
	}
}

void FProxyGenerationProcessor::OnMapChange(uint32 MapFlags)
{
	ClearProcessingData();
}

void FProxyGenerationProcessor::OnNewCurrentLevel()
{
	ClearProcessingData();
}

void FProxyGenerationProcessor::ClearProcessingData()
{
	FScopeLock Lock(&StateLock);
	ProxyMeshJobs.Empty();
	ToProcessJobDataMap.Empty();
}

void FProxyGenerationProcessor::ProcessJob(const FGuid& JobGuid, FProxyGenerationData* Data)
{
	TArray<UObject*> OutAssetsToSync;
	const FString AssetBaseName = FPackageName::GetShortName(Data->MergeData->ProxyBasePackageName);
	const FString AssetBasePath = Data->MergeData->InOuter ? TEXT("") : FPackageName::GetLongPackagePath(Data->MergeData->ProxyBasePackageName) + TEXT("/");

	// Retrieve flattened material data
	FFlattenMaterial& FlattenMaterial = Data->Material;

	// Resize flattened material
	FMaterialUtilities::ResizeFlattenMaterial(FlattenMaterial, Data->MergeData->InProxySettings);

	// Optimize flattened material
	FMaterialUtilities::OptimizeFlattenMaterial(FlattenMaterial);

	// Create a new proxy material instance
	UMaterialInstanceConstant* ProxyMaterial = ProxyMaterialUtilities::CreateProxyMaterialInstance(Data->MergeData->InOuter, Data->MergeData->InProxySettings.MaterialSettings, FlattenMaterial, AssetBasePath, AssetBaseName, OutAssetsToSync);

	// Set material static lighting usage flag if project has static lighting enabled
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);
	if (bAllowStaticLighting)
	{
		ProxyMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting);
	}

	// Construct proxy static mesh
	UPackage* MeshPackage = Data->MergeData->InOuter;
	FString MeshAssetName = TEXT("SM_") + AssetBaseName;
	if (MeshPackage == nullptr)
	{
		MeshPackage = CreatePackage(NULL, *(AssetBasePath + MeshAssetName));
		MeshPackage->FullyLoad();
		MeshPackage->Modify();
	}

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(MeshPackage, FName(*MeshAssetName), RF_Public | RF_Standalone);
	StaticMesh->InitResources();

	FString OutputPath = StaticMesh->GetPathName();

	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = Data->MergeData->InProxySettings.LightMapResolution;
	StaticMesh->LightMapCoordinateIndex = 1;

	FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
	/*Don't allow the engine to recalculate normals*/
	SrcModel->BuildSettings.bRecomputeNormals = false;
	SrcModel->BuildSettings.bRecomputeTangents = false;
	SrcModel->BuildSettings.bRemoveDegenerates = true;
	SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
	SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
	SrcModel->RawMeshBulkData->SaveRawMesh(Data->RawMesh);

	//Assign the proxy material to the static mesh
	StaticMesh->StaticMaterials.Add(FStaticMaterial(ProxyMaterial));

	//Set the Imported version before calling the build
	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

	StaticMesh->Build();
	StaticMesh->PostEditChange();

	OutAssetsToSync.Add(StaticMesh);

	// Execute the delegate received from the user
	Data->MergeData->CallbackDelegate.ExecuteIfBound(JobGuid, OutAssetsToSync);
}
