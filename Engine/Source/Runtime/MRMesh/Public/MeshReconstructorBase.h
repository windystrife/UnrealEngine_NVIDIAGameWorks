// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "MeshReconstructorBase.generated.h"

class IMRMesh;
class UMRMeshComponent;

USTRUCT(BlueprintType)
struct FMRMeshConfiguration
{
	GENERATED_BODY()

	bool bSendVertexColors = false;
};

UCLASS(meta=(Experimental))
class MRMESH_API UMeshReconstructorBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	virtual void StartReconstruction();

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	virtual void StopReconstruction();

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	virtual void PauseReconstruction();

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	virtual bool IsReconstructionStarted() const;

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	virtual bool IsReconstructionPaused() const;

	UFUNCTION()
	virtual FMRMeshConfiguration ConnectMRMesh(UMRMeshComponent* Mesh);

	UFUNCTION()
	virtual void DisconnectMRMesh();

};