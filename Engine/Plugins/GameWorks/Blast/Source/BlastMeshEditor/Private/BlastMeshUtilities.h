// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "BlastFracture.h"


class UStaticMesh;
class UBlastMesh;
struct FSkeletalMaterial;
struct FRawMesh;
class UMaterialInterface;

namespace Nv
{
	namespace Blast
	{
		class Mesh;
		struct AuthoringResult;
	}
}

void BuildSmoothingGroups(FRawMesh& RawMesh);

Nv::Blast::Mesh* CreateAuthoringMeshFromRawMesh(const FRawMesh& RawMesh, const FTransform& UE4ToBlastTransform);

void CreateSkeletalMeshFromAuthoring(TSharedPtr<FFractureSession> FractureSession, UStaticMesh* SourceMesh);

void CreateSkeletalMeshFromAuthoring(TSharedPtr<FFractureSession> FractureSession, bool isFinal, UMaterialInterface* InteriorMaterial);

void UpdateSkeletalMeshFromAuthoring(TSharedPtr<FFractureSession> FractureSession, UMaterialInterface* InteriorMaterial);


