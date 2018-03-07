// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BlastFractureSettings.h"
#include "BlastChunkParamsProxy.generated.h"

class IBlastMeshEditor;
class UBlastMesh;

UCLASS()
class UBlastChunkParamsProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UBlastMesh* BlastMesh;

	UPROPERTY(VisibleAnywhere, Category = Chunks)
	int32 ChunkIndex;

	UPROPERTY(VisibleAnywhere, Category = Chunks)
	FVector ChunkCentroid;

	UPROPERTY(VisibleAnywhere, Category = Chunks)
	float ChunkVolume;

	//UPROPERTY(EditAnywhere, Category=Chunks)
	//FBlastChunkParameters ChunkParams;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	TWeakPtr<IBlastMeshEditor> BlastMeshEditorPtr;
#endif
};
