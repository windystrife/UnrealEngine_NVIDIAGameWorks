// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DestructibleFractureSettings.h"
#include "DestructibleChunkParamsProxy.generated.h"

class IDestructibleMeshEditor;
class UDestructibleMesh;

UCLASS()
class UDestructibleChunkParamsProxy : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UDestructibleMesh* DestructibleMesh;

	UPROPERTY()
	int32 ChunkIndex;

	UPROPERTY(EditAnywhere, Category=Chunks)
	FDestructibleChunkParameters ChunkParams;

#if WITH_EDITOR
	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface

	TWeakPtr<IDestructibleMeshEditor> DestructibleMeshEditorPtr;
#endif
};
