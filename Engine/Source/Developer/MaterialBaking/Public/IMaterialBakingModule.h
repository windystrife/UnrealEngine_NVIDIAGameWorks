// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "PixelFormat.h"
#include "SceneTypes.h"

class UTextureRenderTarget2D;
class UMaterialOptions;
class UMaterialInterface;

class FExportMaterialProxy;
class FTextureRenderTargetResource;

struct FMaterialData; 
struct FMeshData;
struct FBakeOutput;

class MATERIALBAKING_API IMaterialBakingModule : public IModuleInterface
{
public:
	/** Bakes out material properties according to MaterialSettings using MeshSettings and stores the output in Output */
	virtual void BakeMaterials(const TArray<FMaterialData*>& MaterialSettings, const TArray<FMeshData*>& MeshSettings, TArray<FBakeOutput>& Output) = 0;

	/** Promps a slate window to allow the user to populate specific material baking settings used while baking out materials */
	virtual  bool SetupMaterialBakeSettings(TArray<TWeakObjectPtr<UObject>>& OptionObjects, int32 NumLODs) = 0;
};
