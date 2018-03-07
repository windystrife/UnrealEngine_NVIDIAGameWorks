// Copyight 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshReconstructorBase.h"

#include "DummyMeshReconstructor.generated.h"

class FDummyMeshReconstructor;

UCLASS(BlueprintType, meta = (Experimental))
class DUMMYMESHRECONSTRUCTOR_API UDummyMeshReconstructor : public UMeshReconstructorBase
{
	GENERATED_BODY()

public:
	//~ UMeshReconstructorBase
	UFUNCTION(BlueprintCallable, Category="Mesh Reconstruction")
	virtual void StartReconstruction() override;
	
	UFUNCTION(BlueprintCallable, Category="Mesh Reconstruction")
	virtual void StopReconstruction() override;
	
	UFUNCTION(BlueprintCallable, Category="Mesh Reconstruction")
	virtual void PauseReconstruction() override;
	
	UFUNCTION(BlueprintCallable, Category="Mesh Reconstruction")
	virtual bool IsReconstructionStarted() const override;
	
	UFUNCTION(BlueprintCallable, Category="Mesh Reconstruction")
	virtual bool IsReconstructionPaused() const override;

	virtual FMRMeshConfiguration ConnectMRMesh(UMRMeshComponent* Mesh) override;

	virtual void DisconnectMRMesh() override;
	//~ UMeshReconstructorBase

private:
	void EnsureImplExists();
	TSharedPtr<FDummyMeshReconstructor> ReconstructorImpl;
};